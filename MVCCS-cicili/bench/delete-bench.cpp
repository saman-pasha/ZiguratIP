// The delete side of the engine, measured: N rows under ONE index key --
// the shape of cocolog's kb and name indexes, where every clause of a
// predicate shares the key and the values form one chain -- deleted in
// three rounds, so the second and third run against the dead links the
// earlier rounds left in the chain (a knowledge base with history):
//
//   G   N live, delete all
//   H   again, with N dead links in the chain
//   I   again, with 2N dead links
//
// and four ways of choosing the rows, the second argument:
//
//   (none)   page-cursor order, oldest first -- NOT the chain's order,
//            so the unmap resume mark misses on every page boundary
//   newest   page-cursor order reversed
//   equal    rows yielded by cursor_equal and deleted in its callback:
//            the exact shape of cocolog's forget_clauses; chain order,
//            so the mark should make every unmap O(1)
//   equal2   equal, plus a UNIQUE index keyed by the row id -- the
//            server's shape (clauses carry a sequence PRIMARY KEY)
//   equal3   equal2 with the id keys multiplied by 64: the control --
//            same two indexes, but keys that never touched the chain's
//            slot in the old `key & 63' mark table
//
// equal vs equal2 vs equal3 is the measurement that found the mark
// eviction (see the UnmapMark comment in mvccs-lib.cicili). Built and
// run by ./build.sh; `delete_bench N equal2` for another N.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <chrono>
#include "engine.hpp"
#include "filestream.hpp"
static uint8_t ITEM_KEY[20] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20 };
static uint8_t IDX_KEY[20]  = { 21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40 };
static BTreeIndex IDX, IDX2; static Memory* g_m; static bool two_idx = false; static bool slot_safe = false;
static uint8_t IDX2_KEY[20] = { 41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60 };
struct Item : public BaseTable {
  int64_t id = 0; int64_t value = 0;
  int64_t pack_size () override { return 16; }
  void pack (Zigurat::binarystream& io) override { io.write_std_long(id); io.write_std_long(value); }
  void unpack (Zigurat::binarystream& io) override { io.read_std_long(id); io.read_std_long(value); }
  void map (void*) override { if (two_idx) bt_map(&IDX2, slot_safe ? id * 64 : id, pointer.address); bt_map(&IDX, value, pointer.address); }
  void unmap (void*) override { if (two_idx) bt_unmap(&IDX2, slot_safe ? id * 64 : id, pointer.address); bt_unmap(&IDX, value, pointer.address); }
};
static void idx_attach (Memory* m) {
  IDX.m = m; IDX.name = "IDX"; IDX.hash_key = intern_key(IDX_KEY); IDX.table_key = ITEM_KEY;
  IDX.catalogue_id = 424242; IDX.branching_factor = 65; IDX.min_degree = 64; IDX.max_degree = 128;
  IDX.is_unique = 0; IDX.root_address = -1; IDX.record_pointer = pointer_null(); IDX.levels = 1;
  IDX.is_dependent = 0; IDX.dep_hash_key = nullptr; bt_select_record(&IDX);
  IDX2 = IDX; IDX2.name = "IDX2"; IDX2.hash_key = intern_key(IDX2_KEY); IDX2.catalogue_id = 424243; IDX2.is_unique = 1; IDX2.root_address = -1; IDX2.record_pointer = pointer_null(); bt_select_record(&IDX2);
}
static double now () { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static bool collect_cb (void* u, Pointer* p) { ((std::vector<Pointer>*)u)->push_back(*p); return true; }
struct EqCtx { long n; double t0; };
static bool eq_delete_cb (void* u, Pointer* p) {   // the server's forget_clauses shape
  EqCtx* c = (EqCtx*)u; Item it; it.pointer = *p; read_row(g_m, &it); online_delete(g_m, &it);
  c->n++; if (c->n % 1000 == 0) printf("      %ld done, %.2fs\n", c->n, now() - c->t0); return true; }
static double delete_via_equal () {
  EqCtx c{0, now()}; long ks[1] = {42};
  bt_cursor_equal_multi(&IDX, (int64_t*)ks, &c, eq_delete_cb);
  double t = now() - c.t0; commit_transaction(g_m); begin_transaction(g_m);
  printf("   deleted %ld rows via cursor_equal in %.2fs (%.3f ms/row)\n", c.n, t, t / (c.n ? c.n : 1) * 1e3); return t; }
static void insert_n (long n) { for (long i = 1; i <= n; i++) { Item it; it.id = i; it.value = 42; online_insert(g_m, ITEM_KEY, &it); } commit_transaction(g_m); begin_transaction(g_m); }
static double delete_all (bool newest_first) {
  std::vector<Pointer> ps; engine_cursor(g_m, ITEM_KEY, &ps, collect_cb);
  double t0 = now();
  if (newest_first) for (long i = (long)ps.size() - 1; i >= 0; i--) { Item it; it.pointer = ps[i]; read_row(g_m, &it); online_delete(g_m, &it); if ((ps.size()-i) % 1000 == 0) printf("      %ld done, %.2fs\n", (long)(ps.size()-i), now()-t0); }
  else for (size_t i = 0; i < ps.size(); i++) { Item it; it.pointer = ps[i]; read_row(g_m, &it); online_delete(g_m, &it); if ((i+1) % 1000 == 0) printf("      %zu done, %.2fs\n", i+1, now()-t0); }
  double t = now() - t0; commit_transaction(g_m); begin_transaction(g_m);
  printf("   deleted %zu rows in %.2fs (%.3f ms/row)\n", ps.size(), t, t / ps.size() * 1e3); return t;
}
int main (int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  long n = argc > 1 ? atol(argv[1]) : 7000; bool newest = argc > 2; bool via_eq = argc > 2 && std::string(argv[2]).rfind("equal",0) == 0; two_idx = argc > 2 && (std::string(argv[2]) == "equal2" || std::string(argv[2]) == "equal3"); slot_safe = argc > 2 && std::string(argv[2]) == "equal3";
  printf("indexes: %s\n", two_idx ? "kb-like chain + unique id (the server shape)" : "kb-like chain only");
  remove("/tmp/mvccs-delete-bench-hexmap.bin"); remove("/tmp/mvccs-delete-bench-data.bin");
  Zigurat::filestream h(std::string("/tmp/mvccs-delete-bench-hexmap.bin"), std::ios::in | std::ios::out | std::ios::trunc);
  Zigurat::filestream d(std::string("/tmp/mvccs-delete-bench-data.bin"), std::ios::in | std::ios::out | std::ios::trunc);
  g_m = engine_memory_new(); memory_open(g_m, (Zigurat::binarystream*)&h, (Zigurat::binarystream*)&d, 8192);
  idx_attach(g_m); begin_transaction(g_m);
  printf("G  one key, %ld live, delete all (%s first):\n", n, newest ? "newest" : "oldest"); insert_n(n); if (via_eq) delete_via_equal(); else delete_all(newest);
  printf("H  ... again with %ld dead links in the chain:\n", n); insert_n(n); if (via_eq) delete_via_equal(); else delete_all(newest);
  printf("I  ... again with %ld dead links in the chain:\n", 2*n); insert_n(n); if (via_eq) delete_via_equal(); else delete_all(newest);
  commit_transaction(g_m); engine_memory_delete(g_m); return 0; }
