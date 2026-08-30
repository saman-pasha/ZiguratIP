// The insert side of the engine, measured: N rows against libMVCCS.so in
// consumer-test's shape, each scenario on a fresh store.
//
//   A   no index, one transaction          -- the row itself
//   A'  the same at N/4                     -- does the row's cost grow with N?
//   B   one index, branching 3             -- a deep tree of tiny nodes
//   C   no index, one transaction per row  -- the commit's three syncs
//   D   index at branching 65, SEQUENTIAL keys -- the server's PRIMARY KEY
//       from a sequence (Compiler/compilerddl.cpp gives every index 65)
//   E   D, unique
//   F   branching 65, shuffled keys
//
// The first and third quarters are timed separately: a per-row cost that
// grows with N shows there. Built and run by ./build.sh; `insert_bench N`
// for another N (default 7000).
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <chrono>
#include "engine.hpp"
#include "filestream.hpp"

static uint8_t ITEM_KEY[20] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20 };
static uint8_t IDX_KEY[20]  = { 21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40 };
static BTreeIndex IDX;
static bool use_index = false;
static bool seq_keys = false;
static int BF=3, MIND=2, MAXD=4, UNIQ=0;

struct Item : public BaseTable {
  int64_t id = 0;
  int64_t value = 0;
  int64_t pack_size () override { return 16; }
  void pack (Zigurat::binarystream& io) override { io.write_std_long(id); io.write_std_long(value); }
  void unpack (Zigurat::binarystream& io) override { io.read_std_long(id); io.read_std_long(value); }
  void map (void*) override { if (use_index) bt_map(&IDX, value, pointer.address); }
  void unmap (void*) override { if (use_index) bt_unmap(&IDX, value, pointer.address); }
};

static void idx_attach (Memory* m) {
  IDX.m = m; IDX.name = "IDX"; IDX.hash_key = intern_key(IDX_KEY); IDX.table_key = ITEM_KEY;
  IDX.catalogue_id = 424242; IDX.branching_factor = BF; IDX.min_degree = MIND;
  IDX.max_degree = MAXD; IDX.is_unique = UNIQ; IDX.root_address = -1; IDX.record_pointer = pointer_null(); IDX.levels = 1;
  IDX.is_dependent = 0; IDX.dep_hash_key = nullptr;
  bt_select_record(&IDX);
}

static double now () {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void run (const char* label, long n, bool index, bool per_row_txn) {
  remove("/tmp/mvccs-insert-bench-hexmap.bin"); remove("/tmp/mvccs-insert-bench-data.bin");
  Zigurat::filestream h(std::string("/tmp/mvccs-insert-bench-hexmap.bin"), std::ios::in | std::ios::out | std::ios::trunc);
  Zigurat::filestream d(std::string("/tmp/mvccs-insert-bench-data.bin"), std::ios::in | std::ios::out | std::ios::trunc);
  Memory* m = engine_memory_new();
  memory_open(m, (Zigurat::binarystream*)&h, (Zigurat::binarystream*)&d, 8192);
  use_index = index;
  if (index) idx_attach(m);
  begin_transaction(m);
  double t0 = now(), t_first_q = 0, t_last_q = 0;
  for (long i = 1; i <= n; i++) {
    Item it; it.id = i; it.value = seq_keys ? i : (i * 7919) % 10007;   // shuffled keys for the tree
    online_insert(m, ITEM_KEY, &it);
    if (per_row_txn) { commit_transaction(m); begin_transaction(m); }
    if (i == n / 4) t_first_q = now() - t0;
    if (i == 3 * n / 4) t_last_q = now() - t0;
  }
  double t_ins = now() - t0;
  double c0 = now();
  commit_transaction(m);
  double t_commit = now() - c0;
  engine_memory_delete(m);
  printf("%-34s n=%5ld  inserts %7.3fs (%6.3f ms/row)  commit %6.3fs  | first quarter %.3fs, third quarter %.3fs\n",
         label, n, t_ins, t_ins / n * 1e3, t_commit, t_first_q, t_last_q - t_first_q);
}

int main (int argc, char** argv) {
  long n = argc > 1 ? atol(argv[1]) : 7000;
  run("A  no index, one txn", n, false, false);
  run("A' no index, one txn, quarter N", n / 4, false, false);
  run("B  index b=3, one txn", n, true, false);
  run("C  no index, txn per row", n, false, true);
  BF=65; MIND=64; MAXD=128; seq_keys=true;  run("D  index b=65, sequential keys", n, true, false);
  UNIQ=1;                                   run("E  index b=65 UNIQUE, sequential", n, true, false);
  UNIQ=0; seq_keys=false;                   run("F  index b=65, shuffled keys", n, true, false);
  return 0;
}
