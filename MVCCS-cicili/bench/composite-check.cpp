// A correctness pin for the composite index, in the shape cocolog's clauses
// table gives it: rows keyed (kb, name) with several rows per pair, loaded
// the way a consult loads them -- for each name, every row of the pair
// deleted through a cursor_equal_multi walk, then the pair's rows inserted
// again -- and then EVERY pair asked back two ways: the equal descent
// (cursor_equal_multi, which is what clauses_of does) and the outer level's
// full walk (bt_cursor_dep + a dependent cursor, which is what predicates_of
// does). A pair that answers fewer rows one way than the other is the bug
// this file was written to catch: a program of 377 predicates loaded into
// a fresh base and hex_neighbor/3 answered five clauses of six, then none.
//
//   STORE_MAP=1        the store mapped rather than through a filebuf
//   composite_check 3 4000   a table past 1024 pages, where the page
//                            cursor's old fixed snapshot dropped rows
//   MVCCS_NO_CACHE=1   the engine's record cache off
//
// so four runs bracket where a wrong answer comes from.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <map>
#include "engine.hpp"
#include "filestream.hpp"
#include "mapstream.hpp"

static Zigurat::binarystream* open_store (const char* path, bool fresh) {
  const std::ios_base::openmode mode = std::ios::in | std::ios::out | (fresh ? std::ios::trunc : (std::ios_base::openmode)0);
  const char* env = getenv("STORE_MAP");
  if (env && env[0] == '1') return new Zigurat::mapstream(std::string(path), mode);
  return new Zigurat::filestream(std::string(path), mode);
}

static uint8_t ROW_KEY[20] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20 };
static uint8_t IDX_KEY[20] = { 21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40 };
static uint8_t DEP_KEY[20] = { 41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60 };
static BTreeIndex IDX;
static Memory* g_m;

struct Row : public BaseTable {
  int64_t kb = 0; int64_t name = 0; int64_t ordinal = 0;
  int64_t pack_size () override { return 24; }
  void pack (Zigurat::binarystream& io) override { io.write_std_long(kb); io.write_std_long(name); io.write_std_long(ordinal); }
  void unpack (Zigurat::binarystream& io) override { io.read_std_long(kb); io.read_std_long(name); io.read_std_long(ordinal); }
  void map (void*) override { int64_t ks[2] = { kb, name }; bt_map_multi(&IDX, ks, pointer.address); }
  void unmap (void*) override { int64_t ks[2] = { kb, name }; bt_unmap_multi(&IDX, ks, pointer.address); }
};

static void idx_attach (Memory* m) {
  IDX.m = m; IDX.name = "IDX_KB_NAME"; IDX.hash_key = intern_key(IDX_KEY); IDX.table_key = ROW_KEY;
  IDX.catalogue_id = 424244; IDX.is_unique = 0; IDX.branching_factor = 65; IDX.min_degree = 64;
  IDX.max_degree = 128; IDX.root_address = -1; IDX.record_pointer = pointer_null(); IDX.levels = 2;
  IDX.is_dependent = 0; IDX.dep_hash_key = intern_key(DEP_KEY);
  bt_select_record(&IDX);
}

// a string key rides as a hash in the real table; here the numbers stand
// in, spread so neighbouring names do not sit in neighbouring slots
static int64_t hash_of (int64_t n) { return (n * 2654435761LL) & 0x7fffffffffffLL; }

struct Count { long n; };
static bool count_cb (void* u, Pointer* p) { (void)p; ((Count*)u)->n++; return true; }
struct DepCount { int64_t want; long n; };
static bool dep_rows_cb (void* u, Pointer* p) {
  Row r; r.pointer = *p; read_row(g_m, &r);
  DepCount* c = (DepCount*)u; if (r.name == c->want) c->n++; return true; }
static bool dep_cb (void* u, BTreeIndex* dep) { return bt_cursor_rows_deep(dep, u, dep_rows_cb); }

static int failures = 0;

int main (int argc, char** argv) {
  // 3 x 400 x 6 rows of 24 bytes is a table of ~100 pages; 3 x 4000 puts
  // it well past the 1024 pages the page cursor used to stop at
  const long KBS = argc > 1 ? atol(argv[1]) : 3, NAMES = argc > 2 ? atol(argv[2]) : 400, PER = 6;
  setvbuf(stdout, nullptr, _IONBF, 0);
  remove("/tmp/mvccs-composite-hexmap.bin"); remove("/tmp/mvccs-composite-data.bin");
  Zigurat::binarystream* h = open_store("/tmp/mvccs-composite-hexmap.bin", true);
  Zigurat::binarystream* d = open_store("/tmp/mvccs-composite-data.bin", true);
  g_m = engine_memory_new(); memory_open(g_m, h, d, 8192); idx_attach(g_m);
  begin_transaction(g_m);

  // two consults of the same program into each base: the second is the
  // rewrite -- forget every pair through the index, insert it again
  for (int round = 0; round < 2; round++) {
    for (long kb = 1; kb <= KBS; kb++) {
      for (long name = 1; name <= NAMES; name++) {
        int64_t ks[2] = { hash_of(kb), hash_of(name) };
        std::vector<Pointer> old;
        bt_cursor_equal_multi(&IDX, ks, &old, [] (void* u, Pointer* p) -> bool { ((std::vector<Pointer>*)u)->push_back(*p); return true; });
        for (size_t i = 0; i < old.size(); i++) { Row r; r.pointer = old[i]; read_row(g_m, &r); online_delete(g_m, &r); }
        for (long o = 0; o < PER; o++) { Row r; r.kb = hash_of(kb); r.name = hash_of(name); r.ordinal = o; online_insert(g_m, ROW_KEY, &r); }
      }
    }
    commit_transaction(g_m); begin_transaction(g_m);
    long wrong_equal = 0, wrong_walk = 0;
    for (long kb = 1; kb <= KBS; kb++) {
      for (long name = 1; name <= NAMES; name++) {
        int64_t ks[2] = { hash_of(kb), hash_of(name) };
        Count c{0}; bt_cursor_equal_multi(&IDX, ks, &c, count_cb);
        if (c.n != PER) { if (wrong_equal < 3) printf("   equal: kb %ld name %ld answers %ld rows, want %ld\n", kb, name, c.n, PER); wrong_equal++; }
        // the outer walk reads every row of the base per name, so it is
        // sampled -- every 37th name -- or a large N is quadratic
        if (name % 37 == 1) {
          DepCount dc{hash_of(name), 0}; bt_cursor_equal_dep(&IDX, hash_of(kb), &dc, dep_cb);
          if (dc.n != PER) { if (wrong_walk < 3) printf("   walk:  kb %ld name %ld answers %ld rows, want %ld\n", kb, name, dc.n, PER); wrong_walk++; }
        }
      }
    }
    printf("%s round %d: %ld pairs, equal descent wrong for %ld, outer walk wrong for %ld\n",
           (wrong_equal || wrong_walk) ? "FAIL" : "ok  ", round + 1, KBS * NAMES, wrong_equal, wrong_walk);
    if (wrong_equal || wrong_walk) failures++;
  }
  // and the vacuum's shape: dead rows reclaimed, both indexes dropped and
  // rebuilt from the table's own cursor, then every pair asked again --
  // twice, because a rebuild that loses rows loses more each time
  for (int pass = 0; pass < 2; pass++) {
    Count rows{0}; engine_cursor(g_m, ROW_KEY, &rows, count_cb);
    size_t freed = truncate_key(g_m, ROW_KEY);
    bt_drop_storage(&IDX, g_m);
    engine_cursor(g_m, ROW_KEY, nullptr, [] (void*, Pointer* p) -> bool { Row r; r.pointer = *p; read_row(g_m, &r); r.map(nullptr); return true; });
    commit_transaction(g_m); begin_transaction(g_m);
    Count rows2{0}; engine_cursor(g_m, ROW_KEY, &rows2, count_cb);
    long wrong = 0;
    for (long kb = 1; kb <= KBS; kb++)
      for (long name = 1; name <= NAMES; name++) {
        int64_t ks[2] = { hash_of(kb), hash_of(name) };
        Count c{0}; bt_cursor_equal_multi(&IDX, ks, &c, count_cb);
        if (c.n != PER) { if (wrong < 3) printf("   after rebuild: kb %ld name %ld answers %ld rows, want %ld\n", kb, name, c.n, PER); wrong++; }
      }
    printf("%s rebuild %d: %ld live rows before, %zu dead reclaimed, %ld live rows after, equal descent wrong for %ld\n",
           (wrong || rows2.n != KBS * NAMES * PER) ? "FAIL" : "ok  ", pass + 1, rows.n, freed, rows2.n, wrong);
    if (wrong || rows2.n != KBS * NAMES * PER) failures++;
  }
  // THE HOLE THE PAGE WALK TRIPPED ON, pinned. An allocation's first-fit
  // split leaves a free tail inside a page; the in-page walk used to
  // consume a hole's bytes as a control block and read every record after
  // it one phase out of step -- live rows no cursor, no rebuild and no
  // reclaim could see (a knowledge base losing single clauses at every
  // vacuum, hunted on the live store byte by byte). Making the hole is
  // deliberate: a 64-byte-data row (7 chunks with its control) deleted and
  // reclaimed leaves a 7-chunk span; a 16-byte row (4 chunks) allocated
  // into it leaves a 3-chunk tail -- a hole exactly a control block wide,
  // the worst case -- and the row inserted AFTER the wide one lands past
  // the hole in the same page. Every row must still answer.
  {
    struct Wide : public BaseTable {
      int64_t a=0,b=0,c=0,d_=0,e=0,f=0,g=0,h_=0;
      int64_t pack_size () override { return 64; }
      void pack (Zigurat::binarystream& io) override { io.write_std_long(a); io.write_std_long(b); io.write_std_long(c); io.write_std_long(d_); io.write_std_long(e); io.write_std_long(f); io.write_std_long(g); io.write_std_long(h_); }
      void unpack (Zigurat::binarystream& io) override { io.read_std_long(a); io.read_std_long(b); io.read_std_long(c); io.read_std_long(d_); io.read_std_long(e); io.read_std_long(f); io.read_std_long(g); io.read_std_long(h_); }
      void map (void*) override {} void unmap (void*) override {}
    };
    struct Narrow : public BaseTable {
      int64_t v=0, w=0;
      int64_t pack_size () override { return 16; }
      void pack (Zigurat::binarystream& io) override { io.write_std_long(v); io.write_std_long(w); }
      void unpack (Zigurat::binarystream& io) override { io.read_std_long(v); io.read_std_long(w); }
      void map (void*) override {} void unmap (void*) override {}
    };
    static uint8_t HOLE_KEY[20] = { 61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80 };
    Narrow n1; n1.v = 1; online_insert(g_m, HOLE_KEY, &n1);
    Wide w1; w1.a = 2; online_insert(g_m, HOLE_KEY, &w1);
    Narrow n2; n2.v = 3; online_insert(g_m, HOLE_KEY, &n2);
    commit_transaction(g_m); begin_transaction(g_m);
    online_delete(g_m, &w1);
    commit_transaction(g_m); begin_transaction(g_m);
    truncate_key(g_m, HOLE_KEY);            // reclaims the wide row: a 7-chunk span
    Narrow n3; n3.v = 4; online_insert(g_m, HOLE_KEY, &n3);   // takes 4 chunks, leaves a 3-chunk hole
    Narrow n4; n4.v = 5; online_insert(g_m, HOLE_KEY, &n4);
    commit_transaction(g_m); begin_transaction(g_m);
    long seen = 0; long sum = 0;
    engine_cursor(g_m, HOLE_KEY, nullptr, [] (void*, Pointer*) -> bool { return true; });
    struct Acc { long n; long s; } acc{0,0};
    engine_cursor(g_m, HOLE_KEY, &acc, [] (void* u, Pointer* p) -> bool {
      Narrow r; r.pointer = *p; read_row(g_m, &r);
      ((Acc*)u)->n++; ((Acc*)u)->s += r.v; return true; });
    seen = acc.n; sum = acc.s;
    printf("%s hole walk: 4 narrow rows live behind a 3-chunk hole, the cursor sees %ld (values sum %ld, want 13)\n",
           (seen == 4 && sum == 13) ? "ok  " : "FAIL", seen, sum);
    if (!(seen == 4 && sum == 13)) failures++;
  }
  commit_transaction(g_m); engine_memory_delete(g_m); delete h; delete d;
  printf("composite: %s\n", failures ? "WRONG ANSWERS" : "every pair answers the same both ways");
  return failures;
}
