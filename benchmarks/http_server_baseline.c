// =============================================================================
// C Baseline HTTP Server — Equivalent to examples/http_server.salt
// =============================================================================
// Same functionality: kqueue event loop, /health and /echo RPC endpoints.
// Used for LOC and ergonomics comparison.
//
// Build:   clang -O3 -march=native http_server_baseline.c -o http_server_c
// Run:     ./http_server_c
// Test:    curl localhost:8080/health
//          curl "localhost:8080/echo?msg=hello"
// =============================================================================

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define MAX_EVENTS 64
#define BUF_SIZE 4096
#define MAX_HEADERS 32

// --- String View (C equivalent of StringView) ---
typedef struct {
  const char *ptr;
  int64_t len;
} string_view_t;

static string_view_t sv_from(const char *ptr, int64_t len) {
  return (string_view_t){ptr, len};
}

static int sv_eq(string_view_t a, const char *b, int64_t blen) {
  if (a.len != blen)
    return 0;
  return memcmp(a.ptr, b, blen) == 0;
}

static string_view_t sv_slice(string_view_t s, int64_t start, int64_t end) {
  return (string_view_t){s.ptr + start, end - start};
}

static int64_t sv_find_byte(string_view_t s, char c) {
  const char *p = memchr(s.ptr, c, s.len);
  return p ? (p - s.ptr) : -1;
}

// --- HTTP Request ---
typedef struct {
  string_view_t method;
  string_view_t uri;
  string_view_t version;
  int64_t header_count;
  string_view_t header_names[MAX_HEADERS];
  string_view_t header_values[MAX_HEADERS];
} http_request_t;

typedef struct {
  int ok;
  http_request_t request;
  int64_t bytes_consumed;
} parse_result_t;

// --- HTTP Parser ---
static const char *find_crlf(const char *buf, int64_t len) {
  for (int64_t i = 0; i < len - 1; i++) {
    if (buf[i] == '\r' && buf[i + 1] == '\n')
      return buf + i;
  }
  return NULL;
}

static parse_result_t parse_request(const char *buf, int64_t len) {
  parse_result_t result = {0};
  string_view_t input = sv_from(buf, len);

  // Find end of request line
  const char *line_end = find_crlf(buf, len);
  if (!line_end)
    return result;
  int64_t line_len = line_end - buf;

  string_view_t request_line = sv_slice(input, 0, line_len);

  // Parse METHOD SP URI SP VERSION
  int64_t sp1 = sv_find_byte(request_line, ' ');
  if (sp1 < 0)
    return result;

  result.request.method = sv_slice(request_line, 0, sp1);

  string_view_t rest = sv_slice(request_line, sp1 + 1, line_len);
  int64_t sp2 = sv_find_byte(rest, ' ');
  if (sp2 < 0)
    return result;

  result.request.uri = sv_slice(rest, 0, sp2);
  result.request.version = sv_slice(rest, sp2 + 1, rest.len);

  // Parse headers
  const char *cursor = line_end + 2;
  const char *end = buf + len;
  int64_t header_count = 0;

  while (cursor < end && header_count < MAX_HEADERS) {
    if (cursor + 1 < end && cursor[0] == '\r' && cursor[1] == '\n') {
      cursor += 2;
      break;
    }

    const char *hdr_end = find_crlf(cursor, end - cursor);
    if (!hdr_end)
      break;

    const char *colon = memchr(cursor, ':', hdr_end - cursor);
    if (colon) {
      result.request.header_names[header_count] =
          sv_from(cursor, colon - cursor);
      // Skip colon and whitespace
      const char *val_start = colon + 1;
      while (val_start < hdr_end && *val_start == ' ')
        val_start++;
      result.request.header_values[header_count] =
          sv_from(val_start, hdr_end - val_start);
      header_count++;
    }
    cursor = hdr_end + 2;
  }

  result.request.header_count = header_count;
  result.ok = 1;
  result.bytes_consumed = cursor - buf;
  return result;
}

// --- HTTP Response Writer ---
static int64_t write_response(char *buf, int status, const char *content_type,
                              const char *body, int64_t body_len) {
  int64_t pos = 0;

  // Status line
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

  // Content-Type
  pos += sprintf(buf + pos, "Content-Type: %s\r\n", content_type);
  // Content-Length
  pos += sprintf(buf + pos, "Content-Length: %lld\r\n", (long long)body_len);
  // Connection
  memcpy(buf + pos, "Connection: keep-alive\r\n", 23);
  pos += 23;
  // End headers
  memcpy(buf + pos, "\r\n", 2);
  pos += 2;
  // Body
  memcpy(buf + pos, body, body_len);
  pos += body_len;

  return pos;
}

// --- Connection Handler ---
static void handle_client(int fd, int kq, char *recv_buf, char *send_buf) {
  ssize_t n = read(fd, recv_buf, BUF_SIZE);
  if (n <= 0) {
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(kq, &ev, 1, NULL, 0, NULL);
    close(fd);
    return;
  }

  parse_result_t result = parse_request(recv_buf, n);
  if (!result.ok) {
    int64_t resp_len =
        write_response(send_buf, 500, "text/plain", "Bad Request", 11);
    write(fd, send_buf, resp_len);
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(kq, &ev, 1, NULL, 0, NULL);
    close(fd);
    return;
  }

  http_request_t *req = &result.request;
  string_view_t uri = req->uri;

  // Route: /health
  if (sv_eq(uri, "/health", 7)) {
    const char *body = "{\"status\":\"ok\"}";
    int64_t resp_len =
        write_response(send_buf, 200, "application/json", body, 15);
    write(fd, send_buf, resp_len);
    return;
  }

  // Route: /echo?msg=<value>
  if (uri.len >= 5 && memcmp(uri.ptr, "/echo", 5) == 0) {
    string_view_t query = sv_from(uri.ptr + 5, uri.len - 5);
    int64_t qmark = sv_find_byte(query, '?');
    if (qmark >= 0) {
      query = sv_slice(query, qmark + 1, query.len);
    } else if (query.len > 0 && query.ptr[0] == '?') {
      query = sv_slice(query, 1, query.len);
    }

    // Find msg=<value>
    int64_t eq_pos = sv_find_byte(query, '=');
    char json_buf[256];
    int64_t jlen;

    if (eq_pos >= 0) {
      string_view_t value = sv_slice(query, eq_pos + 1, query.len);
      jlen = snprintf(json_buf, sizeof(json_buf), "{\"echo\":\"%.*s\"}",
                      (int)value.len, value.ptr);
    } else {
      jlen = snprintf(json_buf, sizeof(json_buf), "{\"echo\":\"\"}");
    }

    int64_t resp_len =
        write_response(send_buf, 200, "application/json", json_buf, jlen);
    write(fd, send_buf, resp_len);
    return;
  }

  // 404
  const char *body = "{\"error\":\"not found\"}";
  int64_t resp_len =
      write_response(send_buf, 404, "application/json", body, 20);
  write(fd, send_buf, resp_len);
}

// --- Main ---
int main(void) {
  // Create socket
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    perror("socket");
    return 1;
  }

  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
  setsockopt(listen_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));

  struct sockaddr_in addr = {0};
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

  // Non-blocking
  int flags = fcntl(listen_fd, F_GETFL, 0);
  fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK);

  // Create kqueue
  int kq = kqueue();
  if (kq < 0) {
    perror("kqueue");
    return 1;
  }

  // Register listen socket
  struct kevent ev;
  EV_SET(&ev, listen_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
  kevent(kq, &ev, 1, NULL, 0, NULL);

  printf("C HTTP Server listening on port %d\n", PORT);
  printf("Server ready. Waiting for connections...\n");

  char recv_buf[BUF_SIZE];
  char send_buf[BUF_SIZE];
  struct kevent events[MAX_EVENTS];

  // Event loop
  while (1) {
    int n = kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);
    if (n < 0) {
      perror("kevent wait");
      break;
    }

    for (int i = 0; i < n; i++) {
      int fd = (int)events[i].ident;

      if (fd == listen_fd) {
        // Accept all pending
        while (1) {
          int client_fd = accept(listen_fd, NULL, NULL);
          if (client_fd < 0)
            break;

          int cflags = fcntl(client_fd, F_GETFL, 0);
          fcntl(client_fd, F_SETFL, cflags | O_NONBLOCK);
          int copt = 1;
          setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &copt, sizeof(copt));

          struct kevent cev;
          EV_SET(&cev, client_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
          kevent(kq, &cev, 1, NULL, 0, NULL);
        }
      } else {
        handle_client(fd, kq, recv_buf, send_buf);
      }
    }
  }

  close(listen_fd);
  return 0;
}
