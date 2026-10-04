// A correctness pin for the log index (mvccs-lib.cicili, "THE LOG INDEX"):
// a single-level, non-unique index kept as sorted runs written once, whose
// entries name rows by address and leave the rows to say who may see them.
// Every check here answers the same with the log on and off:
//
//   log_check                        the log on (the default)
//   MVCCS_LOG_INDEX=0 log_check      every index a tree
//
// and four more runs, each a process of its own, carry ONE store across
// the switch -- a tree written, read as a log, written as a log, read as a
// tree again -- because the moves across happen at an index's attach:
//
//   MVCCS_LOG_INDEX=0 log_check tree    a fresh store, its index a tree
//   log_check log                       the same store: the tree's entries
//                                       move into a log; more rows
//   log_check reopen                    the log read back from the disk
//   MVCCS_LOG_INDEX=0 log_check back    the log's live rows back into a tree
//
// and two more pin a merge PACED across commits: one left half done while
// cursors, the inserting transaction, a rollback, an unmap_key and the
// vacuum's shape meet it, and one left under way as the process ends,
// which the next process (the second phase) takes up again:
//
//   log_check pace ; log_check pacereopen
//
// What it pins: the inserting transaction sees its own rows through every
// cursor before it commits, another transaction none of them before and all
// after; equality, the four ranges, not-equal and the full walk agree with
// the rows, the walk in key order; a delete and a key-changing update are
// seen by their own transaction at once and by others at the commit; a
// rollback leaves nothing a cursor returns, though its entries may already
// be in a run; hundreds of small commits merge and lose nothing; unmap_key
// hides a key's rows and lets a new one in; a unique index stays a tree
// and still refuses at the insert; the vacuum's shape (truncate_key, the
// storage dropped, the rows mapped again) answers the same; a SNAPSHOT
// reader keeps the version it began with, under that version's key; and a
// 5 000-row transaction is found whole. STORE_MAP=1 for the mapped store.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <thread>
#include <atomic>
#include "engine.hpp"
#include "filestream.hpp"
#include "mapstream.hpp"

static const char* HEX = "/tmp/mvccs-log-hexmap.bin";
static const char* DAT = "/tmp/mvccs-log-data.bin";

static Zigurat::binarystream* open_store (const char* path, bool fresh) {
  const std::ios_base::openmode mode = std::ios::in | std::ios::out | (fresh ? std::ios::trunc : (std::ios_base::openmode)0);
  const char* env = getenv("STORE_MAP");
  if (env && env[0] == '1') return new Zigurat::mapstream(std::string(path), mode);
  return new Zigurat::filestream(std::string(path), mode);
}

static uint8_t ROW_KEY[20] = { 2,4,6,8,10,12,14,16,18,20,22,24,26,28,30,32,34,36,38,40 };
static uint8_t G_KEY[20]   = { 3,5,7,9,11,13,15,17,19,21,23,25,27,29,31,33,35,37,39,41 };
static uint8_t U_KEY[20]   = { 42,44,46,48,50,52,54,56,58,60,62,64,66,68,70,72,74,76,78,80 };
static BTreeIndex G, U;
static Memory* g_m;

struct Row : public BaseTable {
  int64_t grp = 0, uid = 0, val = 0;
  int64_t pack_size () override { return 24; }
  void pack (Zigurat::binarystream& io) override { io.write_std_long(grp); io.write_std_long(uid); io.write_std_long(val); }
  void unpack (Zigurat::binarystream& io) override { io.read_std_long(grp); io.read_std_long(uid); io.read_std_long(val); }
  void map (void*) override { bt_map(&G, grp, pointer.address); bt_map(&U, uid, pointer.address); }
  void unmap (void*) override { bt_unmap(&G, grp, pointer.address); bt_unmap(&U, uid, pointer.address); }
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
  attach(&G, "G", G_KEY, 620001, 0);
  attach(&U, "U", U_KEY, 620002, 1);
}

static bool log_on () { const char* s = getenv("MVCCS_LOG_INDEX"); return !(s && s[0] == '0'); }

static bool count_cb (void* u, Pointer* p) { (void)p; (*(long*)u)++; return true; }
static long eq (int64_t k) { long n = 0; int64_t ks[1] = { k }; bt_cursor_equal_multi(&G, ks, &n, count_cb); return n; }
static long lt (int64_t k) { long n = 0; bt_cursor_less_than(&G, k, &n, count_cb); return n; }
static long le (int64_t k) { long n = 0; bt_cursor_less_than_equal(&G, k, &n, count_cb); return n; }
static long gt (int64_t k) { long n = 0; bt_cursor_greater_than(&G, k, &n, count_cb); return n; }
static long ge (int64_t k) { long n = 0; bt_cursor_greater_than_equal(&G, k, &n, count_cb); return n; }
static long ne (int64_t k) { long n = 0; bt_cursor_not_equal(&G, k, &n, count_cb); return n; }
static long all () { long n = 0; bt_cursor_rows_deep(&G, &n, count_cb); return n; }
static long eq_u (int64_t k) { long n = 0; int64_t ks[1] = { k }; bt_cursor_equal_multi(&U, ks, &n, count_cb); return n; }

// the full walk in key order: every row's group no smaller than the last
struct Order { long rows; int64_t last; int ascending; };
static bool order_cb (void* u, Pointer* p) {
  Order* o = (Order*)u; Row r; r.pointer = *p; read_row(g_m, &r);
  if (o->rows > 0 && r.grp < o->last) o->ascending = 0;
  o->last = r.grp; o->rows++; return true;
}

// another transaction's view, from another thread
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

// a table's TRUNCATE reaches each of its indexes here (defindex's
// NAME_truncate); engine.hpp does not carry it
size_t bt_truncate (BTreeIndex * idx, Memory * m);

static Row insert (int64_t grp, int64_t uid, int64_t val) {
  Row r; r.grp = grp; r.uid = uid; r.val = val;
  online_insert(g_m, ROW_KEY, &r);
  return r;
}

static Row update (Row& old, int64_t grp) {
  Row r = old; r.grp = grp; r.pointer = pointer_null();
  online_update(g_m, ROW_KEY, &old, &r);
  return r;
}

// the vacuum's shape, exactly as ageing-test runs it
static long reclaim () {
  commit_transaction(g_m);
  begin_transaction(g_m);
  long gone = (long)truncate_key(g_m, ROW_KEY);
  bt_drop_storage(&G, g_m);
  bt_drop_storage(&U, g_m);
  engine_cursor(g_m, ROW_KEY, nullptr, [] (void*, Pointer* p) -> bool {
      Row r; r.pointer = *p; read_row(g_m, &r); r.map(nullptr); return true;
    });
  commit_transaction(g_m);
  begin_transaction(g_m);
  return gone;
}

static int main_run () {
  remove(HEX); remove(DAT);
  open_all(true);
  check("the plain index is a log exactly when the log is on", G.root_address == -2 ? 1 : 0, log_on() ? 1 : 0);
  check("the unique index is a tree either way", U.root_address == -2 ? 1 : 0, 0);

  // 1. own rows first, others' after the commit
  begin_transaction(g_m);
  for (int64_t i = 0; i < 100; i++) insert(i % 10, i, i);
  check("own rows by equality before the commit (grp 3)", eq(3), 10);
  check("another transaction sees none of them yet", other([] { return eq(3); }), 0);
  commit_transaction(g_m);
  check("and all of them after the commit", other([] { return eq(3); }), 10);

  // 2. every cursor of the family
  begin_transaction(g_m);
  check("less than 3", lt(3), 30);
  check("less than or equal to 3", le(3), 40);
  check("greater than 7", gt(7), 20);
  check("greater than or equal to 7", ge(7), 30);
  check("not equal to 3", ne(3), 90);
  check("the full walk", all(), 100);
  { Order o{0, 0, 1}; bt_cursor_rows_deep(&G, &o, order_cb);
    check("and it walks in key order", o.ascending, 1); }
  check("the unique index by its key (uid 42)", eq_u(42), 1);

  // 3. a delete, seen by its own transaction at once
  {
    Row rs[10];
    for (int i = 0; i < 10; i++) rs[i] = insert(40, 1000 + i, i);
    commit_transaction(g_m); begin_transaction(g_m);
    for (int i = 0; i < 4; i++) online_delete(g_m, &rs[i]);
    check("deletes hide their rows from their own transaction (grp 40)", eq(40), 6);
    check("not from another yet", other([] { return eq(40); }), 10);
    commit_transaction(g_m); begin_transaction(g_m);
    check("and from everybody after the commit", other([] { return eq(40); }), 6);
  }

  // 4. an update that moves rows to another key
  {
    Row rs[10];
    for (int i = 0; i < 10; i++) rs[i] = insert(50, 2000 + i, i);
    commit_transaction(g_m); begin_transaction(g_m);
    for (int i = 0; i < 3; i++) update(rs[i], 51);
    check("an update leaves the old key in its own transaction (grp 50)", eq(50), 7);
    check("and reaches the new one (grp 51)", eq(51), 3);
    check("another transaction still sees the old (grp 50)", other([] { return eq(50); }), 10);
    commit_transaction(g_m); begin_transaction(g_m);
    check("after the commit, seven under the old key", other([] { return eq(50); }), 7);
    check("and three under the new", other([] { return eq(51); }), 3);
  }

  // 5. a rollback, after a cursor has put its entries in a run
  begin_transaction(g_m);
  for (int i = 0; i < 20; i++) insert(99, 3000 + i, i);
  check("own rows of a transaction about to roll back (grp 99)", eq(99), 20);
  rollback_transaction(g_m);
  begin_transaction(g_m);
  check("nothing of them after the rollback", eq(99), 0);
  insert(98, 3999, 0);
  commit_transaction(g_m);
  check("nor after the next commit", other([] { return eq(99); }), 0);

  // 6. many small commits: the runs merge and lose nothing
  for (int t = 0; t < 300; t++) {
    begin_transaction(g_m);
    for (int i = 0; i < 20; i++) insert(1000 + (t * 20 + i) % 7, 10000 + t * 20 + i, t);
    commit_transaction(g_m);
  }
  begin_transaction(g_m);
  {
    long total = 0;
    for (int64_t k = 1000; k < 1007; k++) total += eq(k);
    check("300 commits of 20 rows, every one found by its key", total, 6000);
    check("group 1003 alone", eq(1003), 857);
    check("the range over them", ge(1000), 6000);
    Order o{0, 0, 1}; bt_cursor_rows_deep(&G, &o, order_cb);
    check("the walk still in key order", o.ascending, 1);
  }

  // 7. a key taken out wholesale, and let in again
  bt_unmap_key(&G, 1001);
  check("unmap_key hides the key's rows (grp 1001)", eq(1001), 0);
  check("and no other key's (grp 1002)", eq(1002), 857);
  insert(1001, 99999, 0);
  check("a row inserted after it is found", eq(1001), 1);
  commit_transaction(g_m);
  check("and by another transaction", other([] { return eq(1001); }), 1);

  // 8. the unique index still refuses at the insert
  begin_transaction(g_m);
  long threw = 0;
  try { insert(7, 5, 0); } catch (...) { threw = 1; }
  check("a duplicate unique key is refused at its insert", threw, 1);
  rollback_transaction(g_m);

  // 9. the vacuum's shape: dead rows reclaimed, the storage dropped, the
  // survivors mapped again
  begin_transaction(g_m);
  {
    long before = all();
    long gone = reclaim();
    check("the vacuum reclaimed the dead (deletes, the old versions, the rollback)", gone > 0 ? 1 : 0, 1);
    check("every live row in the index again, those unmap_key hid among them", all(), before + 857);
    check("by its key (grp 40)", eq(40), 6);
    check("(grp 51)", eq(51), 3);
    check("(grp 1003)", eq(1003), 857);
    check("the rolled back rows stay gone (grp 99)", eq(99), 0);
    check("the unique index too (uid 2005)", eq_u(2005), 1);
  }
  commit_transaction(g_m);

  // 10. a SNAPSHOT reader keeps the version it began with, under that
  // version's key
  {
    begin_transaction(g_m);
    Row r = insert(700, 70000, 1);
    commit_transaction(g_m);
    std::atomic<int> stage{0};
    long before_old = -1, before_new = -1, after_old = -1, after_new = -1;
    std::thread t([&] {
      begin_transaction(g_m); engine_set_isolation(SNAPSHOT);
      before_old = eq(700); before_new = eq(701);
      stage = 1; while (stage.load() != 2) std::this_thread::yield();
      after_old = eq(700); after_new = eq(701);
      commit_transaction(g_m);
    });
    while (stage.load() != 1) std::this_thread::yield();
    begin_transaction(g_m); update(r, 701); commit_transaction(g_m);
    stage = 2; t.join();
    check("a snapshot reader, before the update: the old key", before_old, 1);
    check("and not the new", before_new, 0);
    check("after the update commits, still the old key", after_old, 1);
    check("and still not the new", after_new, 0);
    check("a fresh reader: not the old key", other([] { return eq(700); }), 0);
    check("but the new", other([] { return eq(701); }), 1);
  }

  // 11. a big transaction, found whole
  begin_transaction(g_m);
  for (int64_t i = 0; i < 5000; i++) insert(5000 + i % 50, 200000 + i, i);
  commit_transaction(g_m);
  {
    long total = other([] { long n = 0; for (int64_t k = 5000; k < 5050; k++) n += eq(k); return n; });
    check("every row of a 5000-row transaction by its key", total, 5000);
  }
  // and the space the vacuum reclaimed holds rows again now: an entry left
  // naming a reclaimed address would find one of them
  begin_transaction(g_m);
  check("the rolled back rows' key still answers nothing (grp 99)", eq(99), 0);
  check("the deleted rows' key still six (grp 40)", eq(40), 6);
  check("the updated rows' old key still seven (grp 50)", eq(50), 7);
  check("and from 5000 up, those 5000 rows alone", ge(5000), 5000);
  commit_transaction(g_m);
  return 0;
}

// ---- one store across the switch -------------------------------------
static long expect_all = 0;
static int phase_tree () {
  remove(HEX); remove(DAT);
  open_all(true);
  check("a tree to begin with", G.root_address == -2 ? 1 : 0, 0);
  begin_transaction(g_m);
  Row rs[30];
  for (int i = 0; i < 300; i++) { Row r = insert(i % 30, i, i); if (i < 30) rs[i] = r; }
  commit_transaction(g_m);
  begin_transaction(g_m);
  for (int i = 0; i < 10; i++) online_delete(g_m, &rs[i]);   // groups 0..9 lose one row
  commit_transaction(g_m);
  begin_transaction(g_m);
  check("the tree's rows (grp 3)", eq(3), 9);
  check("(grp 20)", eq(20), 10);
  check("all of them", all(), 290);
  commit_transaction(g_m);
  return 0;
}

static int phase_log () {
  open_all(false);
  check("the store's tree is a log now", G.root_address == -2 ? 1 : 0, 1);
  begin_transaction(g_m);
  check("every row the tree held, found through the log (grp 3)", eq(3), 9);
  check("(grp 20)", eq(20), 10);
  check("all of them", all(), 290);
  check("the deleted stay deleted", lt(10), 90);
  for (int i = 0; i < 100; i++) insert(i % 30, 1000 + i, i);
  commit_transaction(g_m);
  check("and rows written through the log (grp 3)", other([] { return eq(3); }), 13);
  return 0;
}

static int phase_reopen () {
  open_all(false);
  check("still a log, read back from the disk", G.root_address == -2 ? 1 : 0, 1);
  begin_transaction(g_m);
  check("the log's rows after a reopen (grp 3)", eq(3), 13);
  check("all of them", all(), 390);
  check("in key order", [] { Order o{0, 0, 1}; bt_cursor_rows_deep(&G, &o, order_cb); return (long)o.ascending; }(), 1);
  commit_transaction(g_m);
  return 0;
}

// ---- a merge carried across commits ----------------------------------
// Four 5 000-row commits fill tier 1, and its merge -- 20 000 entries --
// is more than the fourth commit's share (twice its entries), so it is left
// half done; what is checked is what a reader sees while it is. Under
// MVCCS_LOG_MERGE=0 the merge is whole and nothing is under way; with every
// index a tree there is no merge at all; the rows answer the same each way.
static bool paced () {
  const char* s = getenv("MVCCS_LOG_MERGE");
  return log_on() && !(s && s[0] == '0');
}
static long pending () { int64_t runs = 0, pend = 0; engine_log_state(&G, &runs, &pend); return (long)pend; }
static long runs () { int64_t r = 0, pend = 0; engine_log_state(&G, &r, &pend); return (long)r; }
static void fill_tier (int64_t base, int64_t uid0) {
  for (int c = 0; c < 4; c++) {
    begin_transaction(g_m);
    for (int64_t i = 0; i < 5000; i++) insert(base + i % 100, uid0 + c * 5000 + i, i);
    commit_transaction(g_m);
  }
}
static long walk_in_order () { Order o{0, 0, 1}; bt_cursor_rows_deep(&G, &o, order_cb); return (long)o.ascending; }

static int phase_pace () {
  remove(HEX); remove(DAT);
  open_all(true);
  const long under_way = paced() ? 1 : 0;
  fill_tier(8000, 300000);
  check("four 5000-row commits leave their tier's merge under way", pending(), under_way);
  begin_transaction(g_m);
  check("while it is: a key's rows (grp 8042)", eq(8042), 200);
  check("a range", lt(8050), 10000);
  check("all of them", ge(8000), 20000);
  check("the walk in key order", walk_in_order(), 1);
  check("another transaction the same", other([] { return ge(8000); }), 20000);
  for (int i = 0; i < 10; i++) insert(8100, 350000 + i, i);
  check("own rows inserted while it is (grp 8100)", eq(8100), 10);
  check("and the merge still under way after their run", pending(), under_way);
  rollback_transaction(g_m);
  begin_transaction(g_m);
  check("rolled back, none of them", eq(8100), 0);

  // unmap_key catches it half done: given up, every run merged without the key
  bt_unmap_key(&G, 8007);
  check("unmap_key gives the merge up", pending(), 0);
  check("the key's rows gone (grp 8007)", eq(8007), 0);
  check("no other key's (grp 8008)", eq(8008), 200);
  check("all of them less one key's", ge(8000), 19800);
  commit_transaction(g_m);
  check("and so for another transaction", other([] { return ge(8000); }), 19800);
  for (int c = 0; c < 4; c++) { begin_transaction(g_m); insert(7900, 360000 + c, 0); commit_transaction(g_m); }
  check("the merges of later commits do not write the key back (grp 8007)", other([] { return eq(8007); }), 0);
  check("nor lose any other", other([] { return ge(8000); }), 19800);

  // an index's own truncate catches another half done: every run merged
  // into one without the dead rows' entries
  fill_tier(8200, 400000);
  check("another tier's merge under way", pending(), under_way);
  begin_transaction(g_m);
  bt_truncate(&G, g_m);
  check("the index's truncate gives it up", pending(), 0);
  check("and keeps every live row (grp 8242)", eq(8242), 200);
  check("all of them", ge(8000), 39800);
  commit_transaction(g_m);

  // and the vacuum's shape
  fill_tier(8200, 450000);
  check("another merge under way", pending(), under_way);
  begin_transaction(g_m);
  long gone = reclaim();
  check("the vacuum reclaimed the rolled back rows", gone, 10);
  check("and gave the merge up", pending(), 0);
  check("every live row in the index again, those unmap_key hid among them", ge(8000), 60000);
  check("(grp 8007)", eq(8007), 200);
  check("(grp 8242)", eq(8242), 400);
  check("(grp 8008)", eq(8008), 200);
  check("the walk in key order", walk_in_order(), 1);
  commit_transaction(g_m);

  // and a third left under way for the next process
  fill_tier(8400, 500000);
  check("a third merge under way as the process ends", pending(), under_way);
  check("every row once (grp 8007)", other([] { return eq(8007); }), 200);
  check("(grp 8242)", other([] { return eq(8242); }), 400);
  check("(grp 8442)", other([] { return eq(8442); }), 200);
  check("all of them", other([] { return ge(8000); }), 80000);
  return 0;
}

static int phase_pace_reopen () {
  open_all(false);
  check("a fresh process has no merge under way", pending(), 0);
  const long before = runs();
  begin_transaction(g_m);
  check("every row of the half-done merge's inputs (grp 8442)", eq(8442), 200);
  check("all of them", ge(8000), 80000);
  check("the walk in key order", walk_in_order(), 1);
  commit_transaction(g_m);
  // small commits take the full set up again and carry it through
  long began = 0, commits = 0;
  while (commits < 20) {
    begin_transaction(g_m); insert(8600, 600000 + commits, 0); commit_transaction(g_m);
    commits++;
    if (pending() > 0) began = 1;
    else if (began) break;
  }
  check("the next commit takes the merge up again", began, paced() ? 1 : 0);
  check("and small commits finish it", pending(), 0);
  if (log_on()) check("in fewer runs than it found", runs() < before + commits ? 1 : 0, 1);
  check("every row still there", other([] { return ge(8000); }), 80000 + commits);
  check("(grp 8442)", other([] { return eq(8442); }), 200);
  remove(HEX); remove(DAT);
  return 0;
}

static int phase_back () {
  open_all(false);
  check("the store's log is a tree again", G.root_address == -2 ? 1 : 0, 0);
  begin_transaction(g_m);
  check("every live row back in the tree (grp 3)", eq(3), 13);
  check("(grp 20)", eq(20), 13);
  check("all of them", all(), 390);
  check("the deleted stay deleted", lt(10), 130);
  insert(3, 9000, 0);
  commit_transaction(g_m);
  check("and the tree takes new rows (grp 3)", other([] { return eq(3); }), 14);
  remove(HEX); remove(DAT);
  return 0;
}

int main (int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  const char* phase = argc > 1 ? argv[1] : "main";
  printf("log_check %s: the log %s\n", phase, log_on() ? "on" : "OFF");
  int rc = 0;
  if (!strcmp(phase, "main")) rc = main_run();
  else if (!strcmp(phase, "tree")) rc = phase_tree();
  else if (!strcmp(phase, "log")) rc = phase_log();
  else if (!strcmp(phase, "reopen")) rc = phase_reopen();
  else if (!strcmp(phase, "back")) rc = phase_back();
  else if (!strcmp(phase, "pace")) rc = phase_pace();
  else if (!strcmp(phase, "pacereopen")) rc = phase_pace_reopen();
  else { printf("unknown phase %s\n", phase); return 2; }
  (void)expect_all;
  printf("\nlog_check %s: %s (%d failures)\n", phase, failures ? "RED" : "all green", failures);
  return failures || rc ? 1 : 0;
}
