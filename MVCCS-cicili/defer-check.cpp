// A correctness pin for deferred index maintenance: a transaction's index
// entries queue during its inserts and are mapped in bulk at its commit
// (mvccs-lib.cicili, "Deferred index maintenance"). Every check here must
// answer the same with the queue on and off:
//
//   defer_check                       the queue on (the default)
//   MVCCS_DEFER_INDEX=0 defer_check   every entry mapped at its insert
//
// What it pins: the inserting transaction sees its own rows through every
// index before it commits, plain and composite; another transaction sees
// none of them before the commit and all of them after; a unique index
// still refuses a duplicate at the insert; a delete and an update of a row
// inserted in the same transaction find its queued entry; a rollback
// leaves nothing behind, not even for the next commit to map; and a large
// transaction's every row is found by its key afterwards. STORE_MAP=1 for
// the mapped store.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <thread>
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
static uint8_t G_KEY[20]   = { 21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40 };
static uint8_t U_KEY[20]   = { 41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60 };
static uint8_t C_KEY[20]   = { 61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80 };
static uint8_t C_DEP[20]   = { 81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100 };
static BTreeIndex G, U, C;
static Memory* g_m;

struct Row : public BaseTable {
  int64_t grp = 0, uid = 0, a = 0, b = 0;
  int64_t pack_size () override { return 32; }
  void pack (Zigurat::binarystream& io) override { io.write_std_long(grp); io.write_std_long(uid); io.write_std_long(a); io.write_std_long(b); }
  void unpack (Zigurat::binarystream& io) override { io.read_std_long(grp); io.read_std_long(uid); io.read_std_long(a); io.read_std_long(b); }
  void map (void*) override {
    bt_map(&G, grp, pointer.address);
    bt_map(&U, uid, pointer.address);
    int64_t ks[2] = { a, b }; bt_map_multi(&C, ks, pointer.address);
  }
  void unmap (void*) override {
    bt_unmap(&G, grp, pointer.address);
    bt_unmap(&U, uid, pointer.address);
    int64_t ks[2] = { a, b }; bt_unmap_multi(&C, ks, pointer.address);
  }
};

static void attach (BTreeIndex* x, const char* name, uint8_t* key, int64_t cat, int64_t uniq, int64_t levels, uint8_t* dep) {
  x->m = g_m; x->name = name; x->hash_key = intern_key(key); x->table_key = ROW_KEY;
  x->catalogue_id = cat; x->is_unique = uniq; x->branching_factor = 3; x->min_degree = 2; x->max_degree = 4;
  x->root_address = -1; x->record_pointer = pointer_null(); x->levels = levels;
  x->is_dependent = 0; x->dep_hash_key = dep ? intern_key(dep) : nullptr;
  bt_select_record(x);
}

static bool count_cb (void* u, Pointer* p) { (void)p; (*(long*)u)++; return true; }
static long count_g (int64_t k) { long n = 0; int64_t ks[1] = { k }; bt_cursor_equal_multi(&G, ks, &n, count_cb); return n; }
static long count_u (int64_t k) { long n = 0; int64_t ks[1] = { k }; bt_cursor_equal_multi(&U, ks, &n, count_cb); return n; }
static long count_c (int64_t a, int64_t b) { long n = 0; int64_t ks[2] = { a, b }; bt_cursor_equal_multi(&C, ks, &n, count_cb); return n; }

// another transaction's view, from another thread
static long other_g (int64_t k) {
  long n = -1;
  std::thread t([&] { begin_transaction(g_m); n = count_g(k); commit_transaction(g_m); });
  t.join();
  return n;
}
static long other_c (int64_t a, int64_t b) {
  long n = -1;
  std::thread t([&] { begin_transaction(g_m); n = count_c(a, b); commit_transaction(g_m); });
  t.join();
  return n;
}

static int failures = 0;
static void check (const char* what, long got, long want) {
  printf("%s %-64s %ld\n", got == want ? "ok  " : "FAIL", what, got);
  if (got != want) { printf("     want %ld\n", want); failures++; }
}

static Row insert (int64_t grp, int64_t uid, int64_t a, int64_t b) {
  Row r; r.grp = grp; r.uid = uid; r.a = a; r.b = b;
  online_insert(g_m, ROW_KEY, &r);
  return r;
}

int main () {
  setvbuf(stdout, nullptr, _IONBF, 0);
  const char* d = getenv("MVCCS_DEFER_INDEX");
  printf("defer_check: the queue %s\n", (d && d[0] == '0') ? "OFF" : "on");
  remove("/tmp/mvccs-defer-hexmap.bin"); remove("/tmp/mvccs-defer-data.bin");
  Zigurat::binarystream* h = open_store("/tmp/mvccs-defer-hexmap.bin", true);
  Zigurat::binarystream* dd = open_store("/tmp/mvccs-defer-data.bin", true);
  g_m = engine_memory_new();
  memory_open(g_m, h, dd, 8192);
  attach(&G, "G", G_KEY, 610001, 0, 1, nullptr);
  attach(&U, "U", U_KEY, 610002, 1, 1, nullptr);
  attach(&C, "C", C_KEY, 610003, 0, 2, C_DEP);

  // 1. own rows before the commit, other transactions' view before and after
  begin_transaction(g_m);
  for (int64_t i = 0; i < 100; i++) insert(i % 10, i, i % 3, i % 5);
  check("own rows through a plain index before the commit (grp 3)", count_g(3), 10);
  check("own rows through a composite index before the commit (1,2)", count_c(1, 2), 7);
  check("own rows through a unique index before the commit (uid 42)", count_u(42), 1);
  check("another transaction sees none of them yet (grp 3)", other_g(3), 0);
  check("nor through the composite (1,2)", other_c(1, 2), 0);
  commit_transaction(g_m);
  check("another transaction sees them after the commit (grp 3)", other_g(3), 10);
  check("and through the composite (1,2)", other_c(1, 2), 7);

  // 2. a unique index refuses at the insert, as before
  begin_transaction(g_m);
  long threw = 0;
  try { insert(500, 5, 0, 0); } catch (...) { threw = 1; }
  check("a duplicate unique key is refused at its insert", threw, 1);
  rollback_transaction(g_m);
  check("and the rolled-back attempt left no trace (grp 500)", other_g(500), 0);

  // 3. delete rows inserted in the same transaction
  begin_transaction(g_m);
  Row rs[10];
  for (int i = 0; i < 10; i++) rs[i] = insert(42, 1000 + i, 7, 7);
  for (int i = 0; i < 5; i++) online_delete(g_m, &rs[i]);
  check("deletes in the inserting transaction found their entries (grp 42)", count_g(42), 5);
  check("and on the composite (7,7)", count_c(7, 7), 5);
  commit_transaction(g_m);
  check("five rows of ten after the commit (grp 42)", other_g(42), 5);
  check("and on the composite (7,7)", other_c(7, 7), 5);

  // 4. update rows inserted in the same transaction to another key
  begin_transaction(g_m);
  for (int i = 0; i < 10; i++) rs[i] = insert(77, 2000 + i, 8, 8);
  for (int i = 0; i < 3; i++) {
    Row nr = rs[i]; nr.grp = 78; nr.b = 9;
    online_update(g_m, ROW_KEY, &rs[i], &nr);
  }
  check("updated away from the old key in the transaction (grp 77)", count_g(77), 7);
  check("and onto the new one (grp 78)", count_g(78), 3);
  check("the composite followed the update (8,9)", count_c(8, 9), 3);
  commit_transaction(g_m);
  check("seven and three after the commit (grp 77)", other_g(77), 7);
  check("(grp 78)", other_g(78), 3);

  // 5. a rollback leaves nothing, not even for the next commit to map
  begin_transaction(g_m);
  for (int i = 0; i < 20; i++) insert(99, 3000 + i, 9, 9);
  check("own rolled-back-to-be rows visible before the rollback (grp 99)", count_g(99), 20);
  rollback_transaction(g_m);
  begin_transaction(g_m);
  insert(98, 4000, 1, 1);
  commit_transaction(g_m);
  check("nothing of the rolled-back rows after the next commit (grp 99)", other_g(99), 0);
  check("while that commit's own row is there (grp 98)", other_g(98), 1);

  // 6. a large transaction: every row found by its key
  begin_transaction(g_m);
  const int64_t N = 5000;
  for (int64_t i = 0; i < N; i++) insert(100000 + (i * 7919) % 997, 10000 + i, 50 + i % 7, i % 11);
  commit_transaction(g_m);
  long total = 0;
  for (int64_t k = 0; k < 997; k++) total += other_g(100000 + k);
  check("every row of a 5000-row transaction found by its key", total, N);
  long ctotal = 0;
  for (int64_t a = 0; a < 7; a++) for (int64_t b = 0; b < 11; b++) ctotal += other_c(50 + a, b);
  check("and by its composite key", ctotal, N);

  engine_memory_delete(g_m);
  printf("\ndefer_check: %s (%d failure%s)\n", failures ? "RED" : "all green", failures, failures == 1 ? "" : "s");
  return failures ? 1 : 0;
}
