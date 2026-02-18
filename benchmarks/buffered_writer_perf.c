// Buffered Writer Performance Benchmark - C
// Tests buffered file I/O throughput with formatted output

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ITERATIONS 1000000
#define BUFFER_SIZE 128

typedef struct {
  int fd;
  char buffer[BUFFER_SIZE];
  size_t pos;
} BufferedWriter;

static void bw_init(BufferedWriter *w, int fd) {
  w->fd = fd;
  w->pos = 0;
}

static void bw_flush(BufferedWriter *w) {
  if (w->pos > 0) {
    write(w->fd, w->buffer, w->pos);
    w->pos = 0;
  }
}

static void bw_write_bytes(BufferedWriter *w, const char *data, size_t len) {
  if (w->pos + len > BUFFER_SIZE) {
    bw_flush(w);
  }
  if (len <= BUFFER_SIZE) {
    memcpy(w->buffer + w->pos, data, len);
    w->pos += len;
  } else {
    write(w->fd, data, len);
  }
}

static void bw_write_str(BufferedWriter *w, const char *str) {
  bw_write_bytes(w, str, strlen(str));
}

static void bw_write_i32(BufferedWriter *w, int32_t val) {
  char buf[16];
  int len = snprintf(buf, sizeof(buf), "%d", val);
  bw_write_bytes(w, buf, len);
}

static void bw_write_i64(BufferedWriter *w, int64_t val) {
  char buf[24];
  int len = snprintf(buf, sizeof(buf), "%lld", (long long)val);
  bw_write_bytes(w, buf, len);
}

int main(void) {
  // Open output file (O_WRONLY | O_CREAT | O_TRUNC)
  int fd = open("/tmp/c_writer_test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return 1;
  }

  BufferedWriter writer;
  bw_init(&writer, fd);

  // Write formatted output
  for (int64_t i = 0; i < ITERATIONS; i++) {
    bw_write_str(&writer, "Item ");
    bw_write_i32(&writer, (int32_t)i);
    bw_write_str(&writer, ": value = ");
    bw_write_i64(&writer, i * 1000);
    bw_write_str(&writer, "\n");
  }

  // Flush remaining buffer
  bw_flush(&writer);

  // Close file
  close(fd);

  return 0;
}
