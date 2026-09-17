// What GROWING a mapped store costs, three ways -- the measurement behind
// "And then the growing itself" in ../README.md.
//
// It uses no engine at all: a mapped file, and the store's own write
// pattern against it. A fresh page is six extending writes -- four of its
// 512-byte hexmap slice (two fill_n and two standalone bytes, which is what
// full_hexmap_at and free_hexmap emit) and two of the page (the hash key,
// then the page's zeros) -- followed by the in-place hexmap rewrites every
// record allocation makes through the mapping. The mix is the point: a
// bench that only appends measures something the store never does.
//
//   A  ftruncate to the exact new length, then memcpy into the map
//   B  pwrite, which appends and extends in one call
//   C  ftruncate a megabyte ahead, memcpy, truncate back    (what mapbuf does)
//
// A is what mapbuf did until a writing process was found spending two
// thirds of itself inside ftruncate. B replaced it and was WITHDRAWN: it
// writes content through a second path, and a store written that way was
// intermittently incomplete to the next process on Linux/ext4
// (saman-pasha/ZiguratIP#32). C is what mapbuf does now -- every byte of
// content through the mapping, the kernel asked only to move the end, and
// the end cut back to what was written at every sync and at close.
//
//   sh bench/build.sh      # builds and runs this with the rest
//   ./grow_bench /tmp/zig-grow-bench

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <ctime>
#include <sys/mman.h>
#include <sys/stat.h>

#define PAGES      2000
#define PAGE_SIZE  8192
#define HEX_SLICE  512
#define RESERVE    (256u * 1024u * 1024u)
#define CHUNK      (1024u * 1024u)

static double now_ms()
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

struct F {
  int    fd;
  char  *base;
  size_t cap;      // the physical length C has grown the file to
  size_t len;      // the logical length: what has been written
};

static void open_f(F *f, const char *path)
{
  ::unlink(path);
  f->fd = ::open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (f->fd < 0) { std::perror("open"); std::exit(1); }
  void *base = ::mmap(nullptr, RESERVE, PROT_READ | PROT_WRITE, MAP_SHARED, f->fd, 0);
  if (base == MAP_FAILED) { std::perror("mmap"); std::exit(1); }
  f->base = (char *)base;
  f->cap = 0;
  f->len = 0;
}

static void close_f(F *f) { ::munmap(f->base, RESERVE); ::close(f->fd); }

// A: the file is exactly as long as what was written, one ftruncate a write
static void write_a(F *f, const char *src, size_t n)
{
  if (::ftruncate(f->fd, (off_t)(f->len + n)) != 0) { std::perror("ftruncate"); std::exit(1); }
  std::memcpy(f->base + f->len, src, n);
  f->len += n;
}

// B: the same exact length, but the extending write IS the growth
static void write_b(F *f, const char *src, size_t n)
{
  if (::pwrite(f->fd, src, n, (off_t)f->len) != (ssize_t)n) { std::perror("pwrite"); std::exit(1); }
  f->len += n;
}

// C: grow a megabyte at a time and memcpy inside it -- fastest, and it
// leaves the file longer than what was written until the truncate back
static void write_c(F *f, const char *src, size_t n)
{
  if (f->len + n > f->cap) {
    size_t want = ((f->len + n + CHUNK - 1) / CHUNK) * CHUNK;
    if (::ftruncate(f->fd, (off_t)want) != 0) { std::perror("ftruncate"); std::exit(1); }
    f->cap = want;
  }
  std::memcpy(f->base + f->len, src, n);
  f->len += n;
}

static void one_page(F *hex, F *data, void (*w)(F *, const char *, size_t),
                     const char *zeros)
{
  w(hex, zeros, 2);                 // full_hexmap_at: the fill
  w(hex, zeros, 1);                 //                 the standalone byte
  w(hex, zeros, HEX_SLICE - 4);     // free_hexmap:    the fill
  w(hex, zeros, 1);                 //                 the standalone byte
  w(data, zeros, 20);               // the page's hash key
  w(data, zeros, PAGE_SIZE - 20);   // and its zeros

  // the record marks: hexmap bytes of this page rewritten IN PLACE through
  // the mapping, as allocate() does for every row
  size_t slice = hex->len - HEX_SLICE;
  for (int k = 0; k < 16; k++) hex->base[slice + 3 + k * 8] = (char)0x90;
}

static void run(const char *what, void (*w)(F *, const char *, size_t),
                const char *dir, bool truncate_back)
{
  char hexpath[512], datapath[512];
  F hex, data;
  char *zeros = (char *)std::calloc(PAGE_SIZE, 1);

  std::snprintf(hexpath, sizeof hexpath, "%s-hexmap.bin", dir);
  std::snprintf(datapath, sizeof datapath, "%s-data.bin", dir);
  open_f(&hex, hexpath);
  open_f(&data, datapath);

  const double t0 = now_ms();
  for (int i = 0; i < PAGES; i++) one_page(&hex, &data, w, zeros);
  if (truncate_back) {
    ::ftruncate(hex.fd, (off_t)hex.len);
    ::ftruncate(data.fd, (off_t)data.len);
  }
  const double t1 = now_ms();

  struct stat sh;
  ::fstat(hex.fd, &sh);
  std::printf("%-38s %8.1f ms  %6.1f us a page   hexmap %ld KB of blocks for %ld KB written\n",
              what, t1 - t0, (t1 - t0) * 1000.0 / PAGES,
              (long)(sh.st_blocks * 512 / 1024), (long)(sh.st_size / 1024));

  close_f(&hex);
  close_f(&data);
  std::free(zeros);
}

int main(int argc, char **argv)
{
  const char *dir = (argc > 1) ? argv[1] : "/tmp/zig-grow-bench";
  std::printf("%d pages of %d bytes, six extending writes each\n\n", PAGES, PAGE_SIZE);
  run("A  ftruncate per extending write", write_a, dir, false);
  run("B  pwrite extends (withdrawn, see #32)", write_b, dir, false);
  run("C  chunked grow (what mapbuf does)", write_c, dir, true);
  return 0;
}
