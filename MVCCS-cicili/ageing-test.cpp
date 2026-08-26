// AGEING IS BOUNDED, and this test is the proof the family has owed
// itself for a while. Every corner of the tree knows the symptom --
// cocolog's CLAUDE.md: "a slow suite is the store ageing"; the Coco
// bench's sixth rule exists because of it -- deleted and superseded
// versions are kept under MVCC, nothing reclaims them on its own, and
// every later read walks past them.
//
// What the engine PROMISES is not that ageing does not exist -- under
// MVCC it must -- but that it is bounded and recoverable:
//
//   1. THE AGEING IS REAL AND THIS TEST CAN SEE IT. Churn with no
//      reclaim, and a full scan measurably slows. A test that could
//      not see the disease could not vouch for the cure.
//   2. THE RECLAIM PASS RETURNS THE COST. One TRUNCATE (the settled
//      dead handed back, the index rebuilt over the survivors -- the
//      exact pass cocolog's vacuum runs) and the scan reads like a
//      fresh store again, with every live row intact.
//   3. RECLAMATION CONVERGES. A second pass straight after the first
//      finds NOTHING -- the vacuum's own run-it-twice check.
//   4. STEADY STATE IS FLAT. Many cycles of churn-then-reclaim, and
//      the last cycle costs what the first did -- scan and index probe
//      both -- where without the reclaim each cycle would stack its
//      dead on all the previous cycles'.
//   5. THE FILE PLATEAUS. Freed pages go back to the allocator and the
//      next cycle's writes land in them, so the store's high-water
//      mark stops moving. This one is byte-deterministic: no clock in
//      it at all.
//
// TIMING HONESTY: wall clock, best-of-five per measurement, and every
// ratio threshold generous (3x) -- the point is to tell FLAT from
// UNBOUNDED (which reads 10x and worse here), never to flake a build
// over scheduler noise. The structural checks (counts, convergence,
// the plateau) carry the parts of the claim a clock cannot.

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <sys/stat.h>
#include <vector>
#include "engine.hpp"
#include "/home/user/ZiguratIP/StreamIO/filestream.hpp"

static const char* HEX_PATH = "/tmp/mvccs-ageing-hexmap.bin";
static const char* DAT_PATH = "/tmp/mvccs-ageing-data.bin";

static uint8_t ITEM_KEY[20] = {0xa9,0xe1,0x96,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17};
static uint8_t IDX_KEY[20]  = {0xa9,0xe1,0x1d,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17};
static BTreeIndex IDX_ITEM_ID;

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
  void map (void*) override   { bt_map(&IDX_ITEM_ID, id, pointer.address); }
  void unmap (void*) override { bt_unmap(&IDX_ITEM_ID, id, pointer.address); }
};

static Memory* MEM = nullptr;

static int failures = 0;
static void check (const char* what, long got, long want) {
  if (got == want) printf("ok   %-56s %ld\n", what, got);
  else { printf("FAIL %-56s got %ld want %ld\n", what, got, want); failures++; }
}
// got must stay under limit -- the ratio checks, with the numbers shown
static void check_under (const char* what, double got, double limit) {
  if (got <= limit) printf("ok   %-56s %.2f <= %.2f\n", what, got, limit);
  else { printf("FAIL %-56s got %.2f limit %.2f\n", what, got, limit); failures++; }
}
static void check_over (const char* what, double got, double floor_) {
  if (got >= floor_) printf("ok   %-56s %.2f >= %.2f\n", what, got, floor_);
  else { printf("FAIL %-56s got %.2f floor %.2f\n", what, got, floor_); failures++; }
}

static double now_ms () {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
}

static long store_bytes () {
  struct stat h, d;
  if (stat(HEX_PATH, &h) != 0 || stat(DAT_PATH, &d) != 0) return -1;
  return (long)(h.st_size + d.st_size);
}

// ---- the workload -----------------------------------------------------------

static const int64_t LIVE = 200;
static long expected_sum = 0;   // tracked beside every update

struct Scan { long rows; long sum; };
static bool scan_cb (void* u, Pointer* p) {
  Scan* s = (Scan*)u;
  Item it; it.pointer = *p;
  read_row(MEM, &it);
  s->rows++; s->sum += it.value;
  return true;
}

struct Find { Item row; bool hit; };
static bool find_cb (void* u, Pointer* p) {
  Find* f = (Find*)u;
  f->row.pointer = *p;
  read_row(MEM, &f->row);
  f->hit = true;
  return false;
}

static Scan scan_once () {
  Scan s{0,0};
  engine_cursor(MEM, ITEM_KEY, &s, scan_cb);
  return s;
}

// best-of-five: the smallest honest wall reading of REPS scans
static double scan_ms () {
  double best = 1.0e18;
  for (int t = 0; t < 5; t++) {
    double t0 = now_ms();
    for (int r = 0; r < 10; r++) scan_once();
    double d = now_ms() - t0;
    if (d < best) best = d;
  }
  return best;
}

// best-of-five of one probe per live id through the index
static double probe_ms () {
  double best = 1.0e18;
  for (int t = 0; t < 5; t++) {
    double t0 = now_ms();
    for (int64_t id = 1; id <= LIVE; id++) {
      int64_t ks[1] = { id };
      Find f{{}, false};
      bt_cursor_equal_multi(&IDX_ITEM_ID, ks, &f, find_cb);
    }
    double d = now_ms() - t0;
    if (d < best) best = d;
  }
  return best;
}

// one cycle of a working store: every live row rewritten three times,
// three hundred transient rows born and deleted -- the dead a real
// session sheds, committed and settled so the reclaim pass may have them
static void churn () {
  for (int round = 0; round < 3; round++) {
    for (int64_t id = 1; id <= LIVE; id++) {
      int64_t ks[1] = { id };
      Find f{{}, false};
      bt_cursor_equal_multi(&IDX_ITEM_ID, ks, &f, find_cb);
      if (!f.hit) { failures++; printf("FAIL churn lost row %ld\n", (long)id); return; }
      Item next; next.id = id; next.value = f.row.value + 1;
      online_update(MEM, ITEM_KEY, &f.row, &next);
      expected_sum += 1;
    }
  }
  std::vector<Item> transients((size_t)300);
  for (int t = 0; t < 300; t++) {
    transients[(size_t)t].id = 100000 + t;
    transients[(size_t)t].value = t;
    online_insert(MEM, ITEM_KEY, &transients[(size_t)t]);
  }
  for (int t = 0; t < 300; t++)
    online_delete(MEM, &transients[(size_t)t]);
  commit_transaction(MEM);
  begin_transaction(MEM);
}

// the reclaim pass, exactly as cocolog's vacuum runs it: the settled
// dead handed back, the index's storage dropped and rebuilt over the
// survivors. Answers how many the truncate reclaimed.
static long reclaim () {
  commit_transaction(MEM);
  begin_transaction(MEM);
  long gone = (long)truncate_key(MEM, ITEM_KEY);
  bt_drop_storage(&IDX_ITEM_ID, MEM);
  struct Remap { };
  engine_cursor(MEM, ITEM_KEY, nullptr, [] (void*, Pointer* p) -> bool {
      Item it; it.pointer = *p;
      read_row(MEM, &it);
      bt_map(&IDX_ITEM_ID, it.id, p->address);
      return true;
    });
  commit_transaction(MEM);
  begin_transaction(MEM);
  return gone;
}

int main ()
{
  remove(HEX_PATH);
  remove(DAT_PATH);
  Zigurat::filestream h(std::string(HEX_PATH), std::ios::in | std::ios::out | std::ios::trunc);
  Zigurat::filestream d(std::string(DAT_PATH), std::ios::in | std::ios::out | std::ios::trunc);
  MEM = engine_memory_new();
  memory_open(MEM, (Zigurat::binarystream*)&h, (Zigurat::binarystream*)&d, 8192);
  globals_set_memory(MEM);
  begin_transaction(MEM);

  IDX_ITEM_ID.m = MEM;
  IDX_ITEM_ID.name = "IDX_AGEING_ID";
  IDX_ITEM_ID.hash_key = intern_key(IDX_KEY);
  IDX_ITEM_ID.table_key = ITEM_KEY;
  IDX_ITEM_ID.catalogue_id = 0xa9e196;
  IDX_ITEM_ID.is_unique = 1;
  IDX_ITEM_ID.branching_factor = 65;
  IDX_ITEM_ID.min_degree = 64;
  IDX_ITEM_ID.max_degree = 128;
  IDX_ITEM_ID.root_address = -1;
  IDX_ITEM_ID.record_pointer = pointer_null();
  IDX_ITEM_ID.levels = 1;
  IDX_ITEM_ID.is_dependent = 0;
  IDX_ITEM_ID.dep_hash_key = nullptr;
  bt_select_record(&IDX_ITEM_ID);

  // ---- the fresh store, measured -------------------------------------------
  for (int64_t id = 1; id <= LIVE; id++) {
    Item it; it.id = id; it.value = id * 10;
    online_insert(MEM, ITEM_KEY, &it);
    expected_sum += id * 10;
  }
  commit_transaction(MEM);
  begin_transaction(MEM);

  scan_ms();                                    // warm-up, discarded
  const double fresh_scan = scan_ms();
  const double fresh_probe = probe_ms();
  printf("     fresh store: scan %.2fms  probe %.2fms\n", fresh_scan, fresh_probe);

  // ---- 1. the disease, visible ---------------------------------------------
  for (int c = 0; c < 6; c++) churn();
  const double aged_scan = scan_ms();
  printf("     after six cycles, NO reclaim: scan %.2fms (%.1fx fresh)\n",
         aged_scan, aged_scan / fresh_scan);
  check_over("ageing without reclaim is real and visible",
             aged_scan / fresh_scan, 2.0);

  // ---- 2. the cure returns the cost ----------------------------------------
  const long reclaimed = reclaim();
  const double cured_scan = scan_ms();
  const double cured_probe = probe_ms();
  printf("     reclaimed %ld dead; scan %.2fms  probe %.2fms\n",
         reclaimed, cured_scan, cured_probe);
  check_over("the reclaim found the dead", (double)reclaimed, 1000.0);
  check_under("one reclaim returns the scan to fresh cost",
              cured_scan / fresh_scan, 3.0);
  check_under("and the index probe with it",
              cured_probe / fresh_probe, 3.0);

  Scan alive = scan_once();
  check("every live row survived the reclaim", alive.rows, (long)LIVE);
  check("with every update's value intact", alive.sum, expected_sum);

  // ---- 3. reclamation converges --------------------------------------------
  check("a second pass straight after finds nothing", reclaim(), 0);

  // ---- 4 & 5. steady state is flat, and the file stops growing -------------
  double first_scan = 0, first_probe = 0, last_scan = 0, last_probe = 0;
  long bytes_early = 0, bytes_final = 0;
  const int CYCLES = 8;
  for (int c = 1; c <= CYCLES; c++) {
    churn();
    reclaim();
    const double s = scan_ms();
    const double p = probe_ms();
    const long b = store_bytes();
    printf("     cycle %d: scan %.2fms  probe %.2fms  store %ld bytes\n", c, s, p, b);
    if (c == 1) { first_scan = s; first_probe = p; }
    if (c == 2) { bytes_early = b; }
    if (c == CYCLES) { last_scan = s; last_probe = p; bytes_final = b; }
  }
  check_under("eight churn+reclaim cycles on: the scan is flat",
              last_scan / first_scan, 3.0);
  check_under("and the index probe is flat",
              last_probe / first_probe, 3.0);
  check_under("the store file plateaus: freed pages are reused",
              (double)bytes_final, (double)bytes_early + 2 * 8192.0);

  Scan final = scan_once();
  check("and the live rows are still all there", final.rows, (long)LIVE);
  check("carrying every committed update", final.sum, expected_sum);
  commit_transaction(MEM);

  printf("\nageing is bounded: %s (%d failures)\n",
         failures == 0 ? "all green" : "RED", failures);
  return failures != 0;
}
