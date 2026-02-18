// C HTTP Server — Fair comparison with Salt (dynamic response building +
// routing) Build: clang -O3 -march=native benchmarks/c_bench_server.c -o
// /tmp/c_bench_server

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BUF_SIZE 4096
#define MAX_EVENTS 64

static int64_t write_i64(char *buf, int64_t pos, int64_t val) {
  char tmp[20];
  int len = 0;
  if (val == 0) {
    tmp[len++] = '0';
  } else {
    int64_t v = val;
    while (v > 0) {
      tmp[len++] = '0' + (v % 10);
      v /= 10;
    }
  }
  for (int i = len - 1; i >= 0; i--) {
    buf[pos++] = tmp[i];
  }
  return pos;
}

static int64_t write_response(char *buf, int status, const char *ct,
                              int64_t ct_len, const char *body,
                              int64_t body_len) {
  int64_t pos = 0;
  if (status == 200) {
    memcpy(buf + pos, "HTTP/1.1 200 OK\r\n", 17);
    pos += 17;
  } else if (status == 404) {
    memcpy(buf + pos, "HTTP/1.1 404 Not Found\r\n", 23);
    pos += 23;
  } else {
    memcpy(buf + pos, "HTTP/1.1 500 Internal Server Error\r\n", 35);
    pos += 35;
  }
  memcpy(buf + pos, "Content-Type: ", 14);
  pos += 14;
  memcpy(buf + pos, ct, ct_len);
  pos += ct_len;
  buf[pos++] = '\r';
  buf[pos++] = '\n';
  memcpy(buf + pos, "Content-Length: ", 16);
  pos += 16;
  pos = write_i64(buf, pos, body_len);
  buf[pos++] = '\r';
  buf[pos++] = '\n';
  memcpy(buf + pos, "Connection: keep-alive\r\n", 23);
  pos += 23;
  buf[pos++] = '\r';
  buf[pos++] = '\n';
  memcpy(buf + pos, body, body_len);
  pos += body_len;
  return pos;
}

static int64_t route(const char *recv_buf, int64_t n, char *send_buf) {
  const char *sp1 = memchr(recv_buf, ' ', n);
  if (!sp1)
    return write_response(send_buf, 500, "text/plain", 10, "Bad Request", 11);

  int64_t sp1_off = sp1 - recv_buf;
  const char *after_method = sp1 + 1;
  int64_t after_len = n - sp1_off - 1;

  const char *sp2 = memchr(after_method, ' ', after_len);
  if (!sp2)
    return write_response(send_buf, 500, "text/plain", 10, "Bad Request", 11);

  int64_t uri_len = sp2 - after_method;

  if (uri_len == 7 && memcmp(after_method, "/health", 7) == 0) {
    return write_response(send_buf, 200, "application/json", 16,
                          "{\"status\":\"ok\"}", 15);
  }

  if (uri_len >= 5 && memcmp(after_method, "/echo", 5) == 0) {
    const char *q = memchr(after_method + 5, '?', uri_len - 5);
    if (q) {
      const char *eq = memchr(q + 1, '=', uri_len - (q + 1 - after_method));
      if (eq) {
        const char *val = eq + 1;
        int64_t val_len = uri_len - (val - after_method);
        return write_response(send_buf, 200, "application/json", 16, val,
                              val_len);
      }
    }
    return write_response(send_buf, 200, "application/json", 16,
                          "{\"echo\":\"\"}", 11);
  }

  return write_response(send_buf, 404, "application/json", 16,
                        "{\"error\":\"not found\"}", 20);
}

int main(void) {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("socket");
    return 1;
  }

  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
  setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(PORT);

  if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return 1;
  }
  if (listen(listen_fd, 4096) < 0) {
    perror("listen");
    return 1;
  }

  int flags = fcntl(listen_fd, F_GETFL, 0);
  fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

  int kq = kqueue();
  if (kq < 0) {
    perror("kqueue");
    return 1;
  }

  struct kevent ev;
  EV_SET(&ev, listen_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
  kevent(kq, &ev, 1, NULL, 0, NULL);

  fprintf(stderr, "C HTTP Server (fair) listening on port %d\n", PORT);

  char recv_buf[BUF_SIZE];
  char send_buf[BUF_SIZE];
  struct kevent events[MAX_EVENTS];

  while (1) {
    int n = kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);
    if (n < 0) {
      perror("kevent");
      break;
    }

    for (int i = 0; i < n; i++) {
      int fd = (int)events[i].ident;

      if (fd == listen_fd) {
        while (1) {
          int cfd = accept(listen_fd, NULL, NULL);
          if (cfd < 0)
            break;
          int cf = fcntl(cfd, F_GETFL, 0);
          fcntl(cfd, F_SETFL, cf | O_NONBLOCK);
          int copt = 1;
          setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &copt, sizeof(copt));
          struct kevent cev;
          EV_SET(&cev, cfd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
          kevent(kq, &cev, 1, NULL, 0, NULL);
        }
      } else {
        ssize_t nr = recv(fd, recv_buf, BUF_SIZE, 0);
        if (nr <= 0) {
          struct kevent dev;
          EV_SET(&dev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
          kevent(kq, &dev, 1, NULL, 0, NULL);
          close(fd);
          continue;
        }

        int64_t resp_len = route(recv_buf, nr, send_buf);
        send(fd, send_buf, resp_len, 0);
      }
    }
  }

  close(listen_fd);
  return 0;
}
