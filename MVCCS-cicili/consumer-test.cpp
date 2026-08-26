// The keystone proof: a consumer compiled by PLAIN g++ against
// engine.hpp -- exactly the shape the Parsi compiler will emit -- talks
// to the one engine instance inside libMVCCS2.so.
//
// What crosses the boundary here is everything the replacement rests
// on: an opaque Memory built and freed by the library; a BaseTable
// subclass whose VIRTUALS the library dispatches (pack/unpack from
// online_insert and read_row); a Pointer kept by value across calls;
// the guarded cursor with a consumer callback; a full
// begin/commit/rollback round; and row_latest across an update chain.
// If this runs green, the rest of the replacement is emission plumbing.

#include <cstdio>
#include <cstring>
#include <string>
#include "engine.hpp"
#include "/home/user/ZiguratIP/StreamIO/filestream.hpp"

// the 20-byte table key a Parsi compile would emit (any stable bytes do
// for the proof; the real emission carries the SHA-1-derived key)
static uint8_t ITEM_KEY[20] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20 };

struct Item : public BaseTable {
  int64_t id = 0;
  int64_t value = 0;
  int64_t pack_size () override { return 16; }
  void pack (Zigurat::binarystream& io) override {
    io.write_std_long(id); io.write_std_long(value);
  }
  void unpack (Zigurat::binarystream& io) override {
    io.read_std_long(id); io.read_std_long(value);
  }
};

static int failures = 0;
static void check (const char* what, long got, long want) {
  if (got == want) printf("ok   %-44s %ld\n", what, got);
  else { printf("FAIL %-44s got %ld want %ld\n", what, got, want); failures++; }
}

struct Scan { long rows; long sum; long want_id; Item row; bool found; };
static bool scan_cb (void* u, Pointer* p) {
  Scan* s = (Scan*)u;
  extern Memory* g_m;
  Item it; it.pointer = *p;
  read_row(g_m, &it);
  s->rows++; s->sum += it.value;
  if (s->want_id != 0 && it.id == s->want_id) { s->row = it; s->found = true; }
  return true;
}
Memory* g_m = nullptr;

int main () {
  remove("/tmp/mvccs-consumer-hexmap.bin");
  remove("/tmp/mvccs-consumer-data.bin");
  { // trunc on the first open: a fresh store is CREATED, exactly as the
    // engine's own smoke test opens its first session
    Zigurat::filestream h(std::string("/tmp/mvccs-consumer-hexmap.bin"),
                          std::ios::in | std::ios::out | std::ios::trunc);
    Zigurat::filestream d(std::string("/tmp/mvccs-consumer-data.bin"),
                          std::ios::in | std::ios::out | std::ios::trunc);
    Memory* m = engine_memory_new();
    g_m = m;
    memory_open(m, (Zigurat::binarystream*)&h, (Zigurat::binarystream*)&d, 8192);
    begin_transaction(m);

    Pointer first; memset(&first, 0, sizeof first);
    for (long i = 1; i <= 5; i++) {
      Item it; it.id = i; it.value = i * 10;
      online_insert(m, ITEM_KEY, &it);
      if (i == 3) first = it.pointer;
    }
    commit_transaction(m);
    begin_transaction(m);

    Scan s{0,0,0,{},false};
    engine_cursor(m, ITEM_KEY, &s, scan_cb);
    check("five rows through the guarded cursor", s.rows, 5);
    check("with the bytes the virtuals packed", s.sum, 150);

    // update row 3 twice, through the same find-then-update shape the
    // generated code uses
    for (int round = 0; round < 2; round++) {
      Scan f{0,0,3,{},false};
      engine_cursor(m, ITEM_KEY, &f, scan_cb);
      Item next; next.id = 3; next.value = f.row.value + 1;
      online_update(m, ITEM_KEY, &f.row, &next);
      commit_transaction(m);
      begin_transaction(m);
    }
    Scan s2{0,0,3,{},false};
    engine_cursor(m, ITEM_KEY, &s2, scan_cb);
    check("still five rows after two updates", s2.rows, 5);
    check("the current version reads back", s2.row.value, 32);

    // the road from the first version to the last inserted one
    Pointer p = first;
    check("row_latest walks the two stamps", (long)row_latest(m, &p), 2);
    Item latest; latest.pointer = p;
    read_row(m, &latest);
    check("and lands on the last inserted version", latest.value, 32);

    // a rollback undoes what a consumer staged
    { Scan f{0,0,3,{},false};
      engine_cursor(m, ITEM_KEY, &f, scan_cb);
      Item next; next.id = 3; next.value = 999;
      online_update(m, ITEM_KEY, &f.row, &next); }
    rollback_transaction(m);
    begin_transaction(m);
    Scan s3{0,0,3,{},false};
    engine_cursor(m, ITEM_KEY, &s3, scan_cb);
    check("a rollback leaves the row where it was", s3.row.value, 32);

    commit_transaction(m);
    engine_memory_delete(m);
  }

  // and the store survives the process: a second Memory over the files
  { Zigurat::filestream h(std::string("/tmp/mvccs-consumer-hexmap.bin"),
                          std::ios::in | std::ios::out);
    Zigurat::filestream d(std::string("/tmp/mvccs-consumer-data.bin"),
                          std::ios::in | std::ios::out);
    Memory* m = engine_memory_new();
    g_m = m;
    memory_open(m, (Zigurat::binarystream*)&h, (Zigurat::binarystream*)&d, 8192);
    begin_transaction(m);
    Scan s{0,0,0,{},false};
    engine_cursor(m, ITEM_KEY, &s, scan_cb);
    check("the consumer's store survives a restart", s.rows, 5);
    commit_transaction(m);
    engine_memory_delete(m);
  }

  if (failures == 0) printf("\nconsumer against libMVCCS2.so: all green\n");
  else printf("\nconsumer: %d FAILED\n", failures);
  return failures;
}
