// A correctness pin for the tail append (mvccs-lib.cicili, "THE TAIL
// APPEND"): a key above every key in a tree is appended where the last
// such key went, without the descent, for as long as the hint recorded
// there still holds. Every answer here is the same with the hint on and
// off:
//
//   tail_check                        the hint on (the default)
//   MVCCS_TAIL_HINT=0 tail_check      every insert descends, as before
//
// and with MVCCS_LOG_INDEX=0 the plain index G is a tree too, mapped in
// bulk at each commit -- the commit's sorted queue reaching the hint. A
// second phase reopens the store in a process of its own:
//
//   tail_check ; tail_check reopen
//
// The unique index P has nodes of two to four keys, so a few thousand ids
// split it constantly and make it deep. What it pins: ascending ids every
// one found and the walk in order, the hint TAKEN (mvccs_tail_appends)
// when on and never when off; a duplicate of the largest key and of a
// middle one refused; keys below the largest inserted among appends; a
// rollback of appended keys, and the same ids inserted again; unmap_key of
// the largest, a middle and the smallest key, appends after each; 286
// unmap_keys whose merges re-split wider than an insert's split; the
// vacuum's shape (truncate, the storage dropped, every row mapped again)
// and appends after it; and a reopened store taking appends. Expected
// answers come from sets kept beside the store, not from counts written
// by hand. STORE_MAP=1 for the mapped store; TAIL_CHECK_VERBOSE=1 lists
// every id a check found missing or extra (the first five otherwise).
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <set>
#include <thread>
#include "engine.hpp"
#include "filestream.hpp"
#include "mapstream.hpp"

// the hint's own counter, exported beside mvccs_cursor_steps
int64_t mvccs_tail_appends ();

static const char* HEX = "/tmp/mvccs-tail-hexmap.bin";
static const char* DAT = "/tmp/mvccs-tail-data.bin";
static const char* ROWS = "/tmp/mvccs-tail-rows.txt";

static Zigurat::binarystream* open_store (const char* path, bool fresh) {
  const std::ios_base::openmode mode = std::ios::in | std::ios::out | (fresh ? std::ios::trunc : (std::ios_base::openmode)0);
  const char* env = getenv("STORE_MAP");
  if (env && env[0] == '1') return new Zigurat::mapstream(std::string(path), mode);
  return new Zigurat::filestream(std::string(path), mode);
}

static uint8_t ROW_KEY[20] = { 7,14,21,28,35,42,49,56,63,70,77,84,91,98,105,112,119,126,133,140 };
static uint8_t P_KEY[20]   = { 9,18,27,36,45,54,63,72,81,90,99,108,117,126,135,144,153,162,171,180 };
static uint8_t G_KEY[20]   = { 11,22,33,44,55,66,77,88,99,110,121,132,143,154,165,176,187,198,209,220 };
static BTreeIndex P, G;
static Memory* g_m;

struct Row : public BaseTable {
  int64_t id = 0, grp = 0, val = 0;
  int64_t pack_size () override { return 24; }
  void pack (Zigurat::binarystream& io) override { io.write_std_long(id); io.write_std_long(grp); io.write_std_long(val); }
  void unpack (Zigurat::binarystream& io) override { io.read_std_long(id); io.read_std_long(grp); io.read_std_long(val); }
  void map (void*) override { bt_map(&P, id, pointer.address); bt_map(&G, grp, pointer.address); }
  void unmap (void*) override { bt_unmap(&P, id, pointer.address); bt_unmap(&G, grp, pointer.address); }
};

static void attach (BTreeIndex* x, const char* name, uint8_t* key, int64_t cat, int64_t uniq) {
  x->m = g_m; x->name = name; x->hash_key = intern_key(key); x->table_key = ROW_KEY;
  x->catalogue_id = cat; x->is_unique = uniq; x->branching_factor = 3; x->min_degree = 2; x->max_degree = 4;
  x->root_address = -1; x->record_pointer = pointer_null(); x->levels = 1;
  x->is_dependent = 0; x->dep_hash_key = nullptr;
  bt_select_record(x);
}

static void open_all (bool fresh) {
  g_m = engine_memory_new();
  memory_open(g_m, open_store(HEX, fresh), open_store(DAT, fresh), 8192);
  attach(&P, "P", P_KEY, 630001, 1);
  attach(&G, "G", G_KEY, 630002, 0);
}

static bool hint_on () { const char* s = getenv("MVCCS_TAIL_HINT"); return !(s && s[0] == '0'); }

static bool count_cb (void* u, Pointer* p) { (void)p; (*(long*)u)++; return true; }
static long eq_p (int64_t k) { long n = 0; int64_t ks[1] = { k }; bt_cursor_equal_multi(&P, ks, &n, count_cb); return n; }
static long eq_g (int64_t k) { long n = 0; int64_t ks[1] = { k }; bt_cursor_equal_multi(&G, ks, &n, count_cb); return n; }
static long lt_p (int64_t k) { long n = 0; bt_cursor_less_than(&P, k, &n, count_cb); return n; }
static long ge_p (int64_t k) { long n = 0; bt_cursor_greater_than_equal(&P, k, &n, count_cb); return n; }
static long all_p () { long n = 0; bt_cursor_rows_deep(&P, &n, count_cb); return n; }

// the full walk of P: every row's id larger than the last
struct Order { long rows; int64_t last; int ascending; };
static bool order_cb (void* u, Pointer* p) {
  Order* o = (Order*)u; Row r; r.pointer = *p; read_row(g_m, &r);
  if (o->rows > 0 && r.id <= o->last) o->ascending = 0;
  o->last = r.id; o->rows++; return true;
}
static long walk_in_order () { Order o{0, 0, 1}; bt_cursor_rows_deep(&P, &o, order_cb); return (long)o.ascending; }

template <typename F> static long other (F f) {
  long n = -1;
  std::thread t([&] { begin_transaction(g_m); n = f(); commit_transaction(g_m); });
  t.join();
  return n;
}

static int failures = 0;
static void check (const char* what, long got, long want) {
  printf("%s %-66s %ld\n", got == want ? "ok  " : "FAIL", what, got);
  if (got != want) { printf("     want %ld\n", want); failures++; }
}

// how many of INSERTS ascending keys the hint took: with nodes of two to
// four keys a leaf has room for an append only between splits, so a
// quarter at least -- and none at all with the hint off
static void check_took (const char* what, long took, long inserts) {
  char buf[160];
  snprintf(buf, sizeof buf, "%s: %ld of them appended by the hint", what, took);
  if (hint_on()) check(buf, took >= inserts / 4 ? 1 : 0, 1);
  else check(buf, took, 0);
}

// what the store should hold: the committed rows, the ids P still maps, and
// the rows a rollback left dead for the vacuum
static std::set<int64_t> rows, mapped;
static long dead_rows = 0;

static void insert (int64_t id) {
  Row r; r.id = id; r.grp = id / 10; r.val = id;
  online_insert(g_m, ROW_KEY, &r);
}

static void insert_run (int64_t from, int64_t to, int64_t step, int per_commit) {
  int n = 0;
  begin_transaction(g_m);
  for (int64_t id = from; id <= to; id += step) {
    insert(id);
    rows.insert(id); mapped.insert(id);
    if (++n % per_commit == 0) { commit_transaction(g_m); begin_transaction(g_m); }
  }
  commit_transaction(g_m);
}

// every id the sets say is mapped found once, none other in the range, the
// walk in order and as long as the set; G's groups as the rows say
static void check_all (const char* label) {
  long missing = 0, extra = 0;
  int64_t top = mapped.empty() ? 0 : *mapped.rbegin();
  for (int64_t k = 0; k <= top + 5; k++) {
    long n = eq_p(k);
    bool want = mapped.count(k) > 0;
    if (want && n != 1) { if (missing < 5 || getenv("TAIL_CHECK_VERBOSE")) printf("     missing %lld (found %ld)\n", (long long)k, n); missing++; }
    if (!want && n != 0) { if (extra < 5 || getenv("TAIL_CHECK_VERBOSE")) printf("     extra %lld (found %ld)\n", (long long)k, n); extra++; }
  }
  char buf[160];
  snprintf(buf, sizeof buf, "%s: every mapped id found once", label); check(buf, missing, 0);
  snprintf(buf, sizeof buf, "%s: and no other", label); check(buf, extra, 0);
  snprintf(buf, sizeof buf, "%s: the walk as long as the set", label); check(buf, all_p(), (long)mapped.size());
  snprintf(buf, sizeof buf, "%s: and in id order", label); check(buf, walk_in_order(), 1);
  long g_bad = 0;
  for (int64_t g = 0; g <= top / 10 + 1; g++) {
    long want = 0;
    for (int64_t id : rows) if (id / 10 == g) want++;
    if (eq_g(g) != want) g_bad++;
  }
  snprintf(buf, sizeof buf, "%s: G's every group as the rows say", label); check(buf, g_bad, 0);
}

// the vacuum's shape, as log-check runs it
static long reclaim () {
  long gone = (long)truncate_key(g_m, ROW_KEY);
  bt_drop_storage(&P, g_m);
  bt_drop_storage(&G, g_m);
  engine_cursor(g_m, ROW_KEY, nullptr, [] (void*, Pointer* p) -> bool {
      Row r; r.pointer = *p; read_row(g_m, &r); r.map(nullptr); return true;
    });
  return gone;
}

static int main_run () {
  remove(HEX); remove(DAT);
  open_all(true);

  // 1. ascending ids: the common insert
  int64_t before = mvccs_tail_appends();
  insert_run(1, 2000, 1, 50);
  long took = (long)(mvccs_tail_appends() - before);
  check_took("2000 ascending ids", took, 2000);
  begin_transaction(g_m);
  check_all("ascending");
  check("a range below (lt 1000)", lt_p(1000), 999);
  check("a range above (ge 1500)", ge_p(1500), 501);
  commit_transaction(g_m);

  // 2. a duplicate of the largest key, right after an append, and of a
  // middle one: both refused
  {
    long threw_top = 0, threw_mid = 0;
    begin_transaction(g_m);
    try { insert(2000); } catch (...) { threw_top = 1; }
    rollback_transaction(g_m);
    begin_transaction(g_m);
    try { insert(1000); } catch (...) { threw_mid = 1; }
    rollback_transaction(g_m);
    check("a duplicate of the largest key refused", threw_top, 1);
    check("a duplicate of a middle key refused", threw_mid, 1);
    // each refused row was written before its index refused it
    dead_rows += 2;
  }

  // 3. keys below the largest among appends: odd ids above, then the even
  // ones between them, then appends again
  insert_run(2001, 2101, 2, 7);
  insert_run(2002, 2100, 2, 5);
  insert_run(2102, 2200, 1, 13);
  begin_transaction(g_m);
  check_all("below the largest among appends");
  commit_transaction(g_m);

  // 4. appended keys rolled back, and the same ids inserted again: their
  // keys stay with dead values, so the second insert descends
  begin_transaction(g_m);
  for (int64_t id = 2201; id <= 2210; id++) insert(id);
  check("own appended rows before a rollback (2205)", eq_p(2205), 1);
  rollback_transaction(g_m);
  dead_rows += 10;
  begin_transaction(g_m);
  check("none of them after it (2205)", eq_p(2205), 0);
  commit_transaction(g_m);
  check("nor for another transaction", other([] { return eq_p(2205); }), 0);
  insert_run(2201, 2210, 1, 10);
  check("the same ids inserted again, found once (2205)", other([] { return eq_p(2205); }), 1);
  begin_transaction(g_m);
  check_all("after a rollback");
  commit_transaction(g_m);

  // 5. unmap_key of the largest, a middle and the smallest key, appends
  // after each
  begin_transaction(g_m);
  bt_unmap_key(&P, 2210); mapped.erase(2210);
  commit_transaction(g_m);
  insert_run(2211, 2300, 1, 9);
  begin_transaction(g_m);
  bt_unmap_key(&P, 1000); mapped.erase(1000);
  bt_unmap_key(&P, 1); mapped.erase(1);
  commit_transaction(g_m);
  insert_run(2301, 2400, 1, 11);
  begin_transaction(g_m);
  check("the unmapped largest key gone (2210)", eq_p(2210), 0);
  check_all("after unmap_key");
  commit_transaction(g_m);

  // 5b. many unmap_keys: underflows merged into a sibling that was full,
  // re-split with more keys than an insert's split ever has -- the upper
  // half's degree must count them (bt_split_node), or keys past the
  // recorded degree go unfound and later inserts land mid-list
  for (int64_t k = 3; k <= 2000; k += 7) {
    begin_transaction(g_m);
    bt_unmap_key(&P, k); mapped.erase(k);
    commit_transaction(g_m);
  }
  insert_run(2401, 2450, 1, 10);
  begin_transaction(g_m);
  check_all("after 286 unmap_keys and appends");
  commit_transaction(g_m);

  // 5c. what makes a hint stale, mixed at the top of the tree: inserts just
  // below the largest key (filling and splitting the rightmost leaf behind
  // the hint's back), unmap_key of the largest key and of its neighbours,
  // appended keys rolled back -- among appends, a commit after each step,
  // from one seed. Ascending inserts alone heal the hint (the leaf splits
  // only when full, and a full hint is never used); this is the mix that
  // would put a stale one in front of an append.
  {
    uint64_t seed = 88172645463325252ULL;
    auto rnd = [&] (uint64_t n) { seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17; return seed % n; };
    int64_t top = 100000;
    long appends = 0;
    int64_t mix0 = mvccs_tail_appends();
    for (int step = 0; step < 3000; step++) {
      uint64_t r = rnd(100);
      begin_transaction(g_m);
      if (r < 55) {
        top += 1 + (int64_t)rnd(4);
        insert(top); rows.insert(top); mapped.insert(top);
        appends++;
        commit_transaction(g_m);
      } else if (r < 85) {
        int64_t k = top - 1 - (int64_t)rnd(12);
        if (k > 100000 && !rows.count(k)) { insert(k); rows.insert(k); mapped.insert(k); }
        commit_transaction(g_m);
      } else if (r < 95) {
        int64_t k = top - (int64_t)rnd(3);
        if (mapped.count(k)) { bt_unmap_key(&P, k); mapped.erase(k); }
        commit_transaction(g_m);
      } else {
        // appended and rolled back: their keys stay, their values die, and
        // the ids are never used again (top moves past them)
        for (int i = 0; i < 3; i++) { top += 1; insert(top); }
        rollback_transaction(g_m);
        dead_rows += 3;
      }
    }
    check_took("the mix's appends", (long)(mvccs_tail_appends() - mix0), appends);
  }
  begin_transaction(g_m);
  check_all("after 3000 mixed steps at the top");
  commit_transaction(g_m);

  // 6. the vacuum's shape: the rolled back rows reclaimed, the storage
  // dropped, every row mapped again -- those unmap_key hid among them --
  // and appends after it
  begin_transaction(g_m);
  long gone = reclaim();
  commit_transaction(g_m);
  mapped = rows;
  check("the vacuum reclaimed every row a rollback left dead", gone, dead_rows);
  before = mvccs_tail_appends();
  const int64_t base = *rows.rbegin();
  insert_run(base + 1, base + 200, 1, 20);
  took = (long)(mvccs_tail_appends() - before);
  check_took("200 appends after the vacuum", took, 200);
  begin_transaction(g_m);
  check_all("after the vacuum");
  commit_transaction(g_m);
  // what the next process is to find
  FILE* f = fopen(ROWS, "w");
  if (f) { for (int64_t id : rows) fprintf(f, "%lld\n", (long long)id); fclose(f); }
  return 0;
}

static int phase_reopen () {
  // the rows and the mapping as main_run left them
  FILE* f = fopen(ROWS, "r");
  long long id = 0;
  while (f && fscanf(f, "%lld", &id) == 1) { rows.insert(id); mapped.insert(id); }
  if (f) fclose(f);
  check("the rows main_run left, read back", rows.empty() ? 0 : 1, 1);
  open_all(false);
  begin_transaction(g_m);
  check_all("reopened");
  commit_transaction(g_m);
  int64_t before = mvccs_tail_appends();
  const int64_t base = *rows.rbegin();
  insert_run(base + 1, base + 200, 1, 25);
  long took = (long)(mvccs_tail_appends() - before);
  check_took("200 appends to a reopened store", took, 200);
  begin_transaction(g_m);
  check_all("appended after the reopen");
  commit_transaction(g_m);
  remove(HEX); remove(DAT); remove(ROWS);
  return 0;
}

int main (int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  const char* phase = argc > 1 ? argv[1] : "main";
  printf("tail_check %s: the hint %s\n", phase, hint_on() ? "on" : "OFF");
  int rc = 0;
  if (!strcmp(phase, "main")) rc = main_run();
  else if (!strcmp(phase, "reopen")) rc = phase_reopen();
  else { printf("unknown phase %s\n", phase); return 2; }
  printf("\ntail_check %s: %s (%d failures)\n", phase, failures ? "RED" : "all green", failures);
  return failures || rc ? 1 : 0;
}
