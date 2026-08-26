// The keystone proof: a consumer compiled by PLAIN g++ against
// engine.hpp -- exactly the shape the Parsi compiler will emit -- talks
// to the one engine instance inside libMVCCS.so.
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
#include "../home/include/filestream.hpp"

// the 20-byte table key a Parsi compile would emit (any stable bytes do
// for the proof; the real emission carries the SHA-1-derived key)
static uint8_t ITEM_KEY[20] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20 };

// the index on Item.value -- an instance held by value and attached the
// way the defindex expansion's attach does, which is exactly what the
// Parsi compiler's emission will write
static uint8_t IDX_KEY[20] = { 21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40 };
static BTreeIndex IDX_ITEM_VALUE;

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
  void map (void*) override {
    bt_map(&IDX_ITEM_VALUE, value, pointer.address);
  }
  void unmap (void*) override {
    bt_unmap(&IDX_ITEM_VALUE, value, pointer.address);
  }
};

static void idx_attach (Memory* m) {
  IDX_ITEM_VALUE.m = m;
  IDX_ITEM_VALUE.name = "IDX_ITEM_VALUE";
  IDX_ITEM_VALUE.hash_key = intern_key(IDX_KEY);
  IDX_ITEM_VALUE.table_key = ITEM_KEY;
  IDX_ITEM_VALUE.catalogue_id = 424242;
  IDX_ITEM_VALUE.is_unique = 0;
  IDX_ITEM_VALUE.branching_factor = 3;
  IDX_ITEM_VALUE.min_degree = 2;
  IDX_ITEM_VALUE.max_degree = 4;
  IDX_ITEM_VALUE.root_address = -1;
  IDX_ITEM_VALUE.record_pointer = pointer_null();
  IDX_ITEM_VALUE.levels = 1;
  IDX_ITEM_VALUE.is_dependent = 0;
  IDX_ITEM_VALUE.dep_hash_key = nullptr;
  bt_select_record(&IDX_ITEM_VALUE);
}

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
    idx_attach(m);
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

    // the index moved with the updates: 30 left it, 32 carries the row
    { Scan e{0,0,0,{},false};
      long ks[1]; ks[0] = 32;
      bt_cursor_equal_multi(&IDX_ITEM_VALUE, (int64_t*)ks, &e, scan_cb);
      check("the index finds the updated value", e.rows, 1);
      Scan e2{0,0,0,{},false};
      ks[0] = 30;
      bt_cursor_equal_multi(&IDX_ITEM_VALUE, (int64_t*)ks, &e2, scan_cb);
      check("and the superseded value left it", e2.rows, 0);
      Scan g{0,0,0,{},false};
      bt_cursor_greater_than(&IDX_ITEM_VALUE, 20, &g, scan_cb);
      check("a range cursor crosses the boundary too", g.rows, 3); }

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
    idx_attach(m);
    begin_transaction(m);
    Scan s{0,0,0,{},false};
    engine_cursor(m, ITEM_KEY, &s, scan_cb);
    check("the consumer's store survives a restart", s.rows, 5);
    Scan e{0,0,0,{},false};
    long ks[1]; ks[0] = 32;
    bt_cursor_equal_multi(&IDX_ITEM_VALUE, (int64_t*)ks, &e, scan_cb);
    check("and the index found its catalogue record", e.rows, 1);
    commit_transaction(m);
    engine_memory_delete(m);
  }

  if (failures == 0) printf("\nconsumer against libMVCCS.so: all green\n");
  else printf("\nconsumer: %d FAILED\n", failures);
  return failures;
}
