// Many threads through the parts of the Cicili engine that the server
// puts many threads through -- Test/test_contention.cpp, ported to the
// replacement. The old suite drove the C++ engine's classes; this one
// drives the EXACT surface a Parsi-compiled object runs on: a table
// shaped byte-for-byte like the pass-2 emission (BaseTable subclass,
// member-pointer statics, engine-compat's Zigurat::BTreeIndex over the
// library's ::BTreeIndex, the lazy attach), with each pooled "connection"
// a std::thread holding its own thread-local transaction, exactly as
// ziguratip's workers do.
//
// The cases keep the old suite's names and assertions:
//
//   readers over an index, alone and under a writer; three indexes
//   agreeing with the table after parallel inserts; the composite
//   index's DEPENDENT level under load (the one path nothing else
//   exercised); a scan and a walk sharing the streams; find-then-update
//   at SERIALIZABLE with no lost increment; a row being rewritten never
//   missing from an index or a scan; a scan counting every row exactly
//   once while a writer commits; a reader never waiting at READ
//   COMMITTED; an isolation level dying with its transaction; the
//   SERIALIZABLE slot coming back, admitting one, and giving up rather
//   than hanging; and a dead transaction's lock breaking on contact.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "engine-compat.hpp"
#include "../home/include/filestream.hpp"

// --- an indexed table, as compilerddl.cpp emits it --------------------------

class Part : public BaseTable
{
private:
  LONG   _ID;
  STRING _KIND;
  LONG   _WEIGHT;
public:
  static std::string name;
  static Zigurat::hashkey_t hash_key;
  Part();
  Part(LONG, STRING, LONG);
  typedef LONG   Part::*ID_t;
  typedef STRING Part::*KIND_t;
  typedef LONG   Part::*WEIGHT_t;
  static ID_t     ID;
  static KIND_t   KIND;
  static WEIGHT_t WEIGHT;
  static Zigurat::BTreeIndex<Part, LONG>         IDX_PART_ID;          // unique
  static Zigurat::BTreeIndex<Part, STRING>       IDX_PART_KIND;        // hashed
  static Zigurat::BTreeIndex<Part, STRING, LONG> IDX_PART_KIND_WEIGHT; // composite
  static uint8_t IDX_PART_ID_keybytes[20];
  static uint8_t IDX_PART_KIND_keybytes[20];
  static uint8_t IDX_PART_KIND_WEIGHT_keybytes[20];
  static uint8_t IDX_PART_KIND_WEIGHT_depbytes[20];
  static void attach_indexes();
  int64_t pack_size() override;
  void prepare() override;
  void pack(Zigurat::binarystream&) override;
  void unpack(Zigurat::binarystream&) override;
  void map(void*) override;
  void unmap(void*) override;
  static void truncate_indexes();
};

std::string Part::name = "Zigurat::Test::Part";
Zigurat::hashkey_t Part::hash_key = {0x3d,0x18,0xc4,0x77,0x9a,0x21,0x0e,0x6b,0xf2,0x55,
                                     0x84,0xab,0x39,0x1c,0xd7,0x60,0x42,0xe8,0x0b,0x93};
Part::ID_t     Part::ID     = &Part::_ID;
Part::KIND_t   Part::KIND   = &Part::_KIND;
Part::WEIGHT_t Part::WEIGHT = &Part::_WEIGHT;
Zigurat::BTreeIndex<Part, LONG>         Part::IDX_PART_ID;
Zigurat::BTreeIndex<Part, STRING>       Part::IDX_PART_KIND;
Zigurat::BTreeIndex<Part, STRING, LONG> Part::IDX_PART_KIND_WEIGHT;
uint8_t Part::IDX_PART_ID_keybytes[20] = {0x11,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
uint8_t Part::IDX_PART_KIND_keybytes[20] = {0x22,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
uint8_t Part::IDX_PART_KIND_WEIGHT_keybytes[20] = {0x33,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
uint8_t Part::IDX_PART_KIND_WEIGHT_depbytes[20] = {0x44,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};

void Part::attach_indexes()
{
  static bool engine_attached = ([] () -> bool {
      Memory* m = ::globals_memory();
      {
        ::BTreeIndex* bt = &Part::IDX_PART_ID.bt;
        bt->m = m;
        bt->name = "IDX_PART_ID";
        bt->hash_key = intern_key(Part::IDX_PART_ID_keybytes);
        bt->table_key = Part::hash_key;
        bt->catalogue_id = engine_key64_bytes_fold(Part::IDX_PART_ID_keybytes);
        bt->is_unique = 1;
        bt->branching_factor = 65;
        bt->min_degree = 64;
        bt->max_degree = 128;
        bt->root_address = -1;
        bt->record_pointer = pointer_null();
        bt->levels = 1;
        bt->is_dependent = 0;
        bt->dep_hash_key = nullptr;
        bt_select_record(bt);
      }
      {
        ::BTreeIndex* bt = &Part::IDX_PART_KIND.bt;
        bt->m = m;
        bt->name = "IDX_PART_KIND";
        bt->hash_key = intern_key(Part::IDX_PART_KIND_keybytes);
        bt->table_key = Part::hash_key;
        bt->catalogue_id = engine_key64_bytes_fold(Part::IDX_PART_KIND_keybytes);
        bt->is_unique = 0;
        bt->branching_factor = 65;
        bt->min_degree = 64;
        bt->max_degree = 128;
        bt->root_address = -1;
        bt->record_pointer = pointer_null();
        bt->levels = 1;
        bt->is_dependent = 0;
        bt->dep_hash_key = nullptr;
        bt_select_record(bt);
      }
      {
        ::BTreeIndex* bt = &Part::IDX_PART_KIND_WEIGHT.bt;
        bt->m = m;
        bt->name = "IDX_PART_KIND_WEIGHT";
        bt->hash_key = intern_key(Part::IDX_PART_KIND_WEIGHT_keybytes);
        bt->table_key = Part::hash_key;
        bt->catalogue_id = engine_key64_bytes_fold(Part::IDX_PART_KIND_WEIGHT_keybytes);
        bt->is_unique = 0;
        bt->branching_factor = 65;
        bt->min_degree = 64;
        bt->max_degree = 128;
        bt->root_address = -1;
        bt->record_pointer = pointer_null();
        bt->levels = 2;
        bt->is_dependent = 0;
        bt->dep_hash_key = intern_key(Part::IDX_PART_KIND_WEIGHT_depbytes);
        bt_select_record(bt);
      }
      return true;
    })();
  (void)engine_attached;
}

static struct Part_engine_registrar_t {
  Part_engine_registrar_t() {
    Part::IDX_PART_ID.ensure = &Part::attach_indexes;
    Part::IDX_PART_KIND.ensure = &Part::attach_indexes;
    Part::IDX_PART_KIND_WEIGHT.ensure = &Part::attach_indexes;
  }
} _Part_engine_registrar;

Part::Part() : _ID(nullptr), _KIND(nullptr), _WEIGHT(nullptr) { }
Part::Part(LONG id, STRING kind, LONG weight) : _ID(id), _KIND(kind), _WEIGHT(weight) { }
void Part::prepare() { }
void Part::map(void*)
{
  Part::attach_indexes();
  bt_map(&Part::IDX_PART_ID.bt, engine_key64(this->_ID), this->pointer.address);
  bt_map(&Part::IDX_PART_KIND.bt, engine_key64(this->_KIND), this->pointer.address);
  int64_t ks[2] = { engine_key64(this->_KIND), engine_key64(this->_WEIGHT) };
  bt_map_multi(&Part::IDX_PART_KIND_WEIGHT.bt, ks, this->pointer.address);
}
void Part::unmap(void*)
{
  Part::attach_indexes();
  bt_unmap(&Part::IDX_PART_ID.bt, engine_key64(this->_ID), this->pointer.address);
  bt_unmap(&Part::IDX_PART_KIND.bt, engine_key64(this->_KIND), this->pointer.address);
  int64_t ks[2] = { engine_key64(this->_KIND), engine_key64(this->_WEIGHT) };
  bt_unmap_multi(&Part::IDX_PART_KIND_WEIGHT.bt, ks, this->pointer.address);
}
void Part::truncate_indexes()
{
  Part::attach_indexes();
  Memory* m = ::globals_memory();
  bt_drop_storage(&Part::IDX_PART_ID.bt, m);
  bt_drop_storage(&Part::IDX_PART_KIND.bt, m);
  bt_drop_storage(&Part::IDX_PART_KIND_WEIGHT.bt, m);
  Globals::memory()->cursor<Part>([] (Part& r) -> bool { r.map(nullptr); return true; });
}
int64_t Part::pack_size()
{
  return Zigurat::binarystream::pack_size(this->_ID, this->_KIND, this->_WEIGHT);
}
void Part::pack(Zigurat::binarystream& io)   { io.pack(this->_ID, this->_KIND, this->_WEIGHT); }
void Part::unpack(Zigurat::binarystream& io) { io.unpack(this->_ID, this->_KIND, this->_WEIGHT); }

// --- the harness ------------------------------------------------------------

static const char* KINDS[] = {"bolt", "nut", "washer", "pin"};
static const int KIND_COUNT = 4;

static Memory* MEM = nullptr;

static int failures = 0;
static void check (const char* what, long got, long want) {
  if (got == want) printf("ok   %-56s %ld\n", what, got);
  else { printf("FAIL %-56s got %ld want %ld\n", what, got, want); failures++; }
}
static void check_str (const char* what, const std::string& got, const std::string& want) {
  if (got == want) printf("ok   %-56s %s\n", what, got.c_str());
  else { printf("FAIL %-56s got [%s] want [%s]\n", what, got.c_str(), want.c_str()); failures++; }
}

static void session (IsolationLevel level = READ_COMMITTED) {
  begin_transaction(MEM);
  engine_isolate(MEM, level);
}

static void fan_out (int count, std::function<void (int)> body) {
  std::vector<std::thread> threads;
  for (int i = 0; i < count; i++) threads.push_back(std::thread(body, i));
  for (size_t i = 0; i < threads.size(); i++) threads[i].join();
}

struct Trouble {
  std::atomic<int> count;
  std::mutex guard;
  std::string first;
  Trouble() : count(0) { }
  void note (const std::string& what) {
    this->count++;
    std::lock_guard<std::mutex> lock(this->guard);
    if (this->first.empty()) this->first = what;
  }
  std::string say () const { return this->first.empty() ? std::string("none") : this->first; }
};

static Part make_part (int64_t id) {
  return Part(LONG(id), STRING(std::string(KINDS[id % KIND_COUNT])), LONG(id * 10));
}

// rows 1..n committed, a fresh transaction left open on the calling thread
static void load (int64_t n) {
  session();
  for (int64_t i = 1; i <= n; i++) {
    Part row = make_part(i);
    Globals::memory()->online_insert(row);
  }
  commit_transaction(MEM);
  begin_transaction(MEM);
}

// the table cleared between cases, indexes rebuilt empty, everything settled
static void reset_table () {
  session();
  commit_transaction(MEM);
  begin_transaction(MEM);
  Globals::memory()->truncate<Part>();
  // truncate reclaims the SETTLED dead; live rows go by delete first
  std::vector<Part> live;
  Globals::memory()->cursor<Part>([&] (Part& r) -> bool { live.push_back(r); return true; });
  for (size_t i = 0; i < live.size(); i++) Globals::memory()->online_delete(live[i]);
  commit_transaction(MEM);
  begin_transaction(MEM);
  Globals::memory()->truncate<Part>();
  commit_transaction(MEM);
}

// --- readers over an index --------------------------------------------------

static void concurrent_index_lookups ()
{
  const int64_t ROWS = 200;
  const int THREADS = 8;
  const int LOOKUPS = 60;
  load(ROWS);
  commit_transaction(MEM);

  Trouble trouble;
  std::atomic<int> wrong(0);
  std::atomic<int> found(0);

  fan_out(THREADS, [&] (int t) {
      try {
        session();
        for (int n = 0; n < LOOKUPS; n++) {
          const int64_t want = ((int64_t)(t * LOOKUPS + n) % ROWS) + 1;
          int hits = 0;
          Part::IDX_PART_ID.cursor_equal(LONG(want), [&] (Part& row) -> bool {
              hits++;
              if ((row.*Part::ID).value() != want) wrong++;
              if ((row.*Part::WEIGHT).value() != want * 10) wrong++;
              return true;
            });
          if (hits != 1) wrong++;
          found += hits;
        }
        commit_transaction(MEM);
      } catch (std::exception& e) {
        trouble.note(e.what());
      } catch (...) {
        trouble.note("unknown");
      }
    });

  check_str("lookups: no thread met trouble", trouble.say(), "none");
  check("lookups: no wrong answers", wrong.load(), 0);
  check("lookups: every lookup hit once", found.load(), THREADS * LOOKUPS);
  reset_table();
}

static void lookups_survive_a_writer ()
{
  const int64_t ROWS = 150;
  const int READERS = 6;
  load(ROWS);
  commit_transaction(MEM);

  Trouble trouble;
  std::atomic<bool> writing(true);
  std::atomic<int> wrong(0);
  std::atomic<int> reads(0);

  std::thread writer([&] () {
      try {
        for (int64_t i = ROWS + 1; i <= ROWS + 120; i++) {
          session();
          Part row = make_part(i);
          Globals::memory()->online_insert(row);
          commit_transaction(MEM);
        }
      } catch (std::exception& e) {
        trouble.note(std::string("writer: ") + e.what());
      }
      writing = false;
    });

  fan_out(READERS, [&] (int t) {
      try {
        session();
        while (writing.load()) {
          const int64_t want = ((int64_t)(reads.load() + t) % ROWS) + 1;
          Part::IDX_PART_ID.cursor_equal(LONG(want), [&] (Part& row) -> bool {
              if ((row.*Part::ID).value() != want) wrong++;
              if ((row.*Part::WEIGHT).value() != want * 10) wrong++;
              return true;
            });
          reads++;
        }
        commit_transaction(MEM);
      } catch (std::exception& e) {
        trouble.note(std::string("reader: ") + e.what());
      }
    });

  writer.join();

  check_str("writer under readers: no trouble", trouble.say(), "none");
  check("writer under readers: no wrong answers", wrong.load(), 0);
  check("writer under readers: readers made progress", reads.load() > 0 ? 1 : 0, 1);
  reset_table();
}

static void every_index_agrees ()
{
  const int THREADS = 6;
  const int64_t PER_THREAD = 25;
  Trouble trouble;

  commit_transaction(MEM);
  fan_out(THREADS, [&] (int t) {
      try {
        session();
        for (int64_t i = 0; i < PER_THREAD; i++) {
          const int64_t id = t * PER_THREAD + i + 1;
          Part row = make_part(id);
          Globals::memory()->online_insert(row);
        }
        commit_transaction(MEM);
      } catch (std::exception& e) {
        trouble.note(e.what());
      }
    });

  check_str("parallel inserts: no trouble", trouble.say(), "none");

  const int64_t TOTAL = THREADS * PER_THREAD;

  session();
  std::set<int64_t> in_table;
  Globals::memory()->cursor<Part>([&] (Part& row) -> bool {
      in_table.insert((row.*Part::ID).value());
      return true;
    });
  check("the table holds every row", (long)in_table.size(), (long)TOTAL);

  int missing = 0, duplicated = 0, stray = 0;
  for (int64_t id = 1; id <= TOTAL; id++) {
    int hits = 0;
    Part::IDX_PART_ID.cursor_equal(LONG(id), [&] (Part& row) -> bool {
        hits++;
        if ((row.*Part::ID).value() != id) stray++;
        return true;
      });
    if (hits == 0) missing++;
    if (hits > 1) duplicated++;
  }
  check("the unique index misses none", missing, 0);
  check("and doubles none", duplicated, 0);
  check("and strays never", stray, 0);

  // the hashed String index: every row of a kind, only those, all in the table
  long by_kind = 0;
  int wrong_kind = 0;
  for (int k = 0; k < KIND_COUNT; k++) {
    Part::IDX_PART_KIND.cursor_equal(STRING(std::string(KINDS[k])), [&] (Part& row) -> bool {
        by_kind++;
        if ((row.*Part::KIND).to_std_string() != KINDS[k]) wrong_kind++;
        if (in_table.find((row.*Part::ID).value()) == in_table.end()) wrong_kind++;
        return true;
      });
  }
  check("the hashed index carries every row", by_kind, (long)TOTAL);
  check("and only rows of the kind asked", wrong_kind, 0);
  commit_transaction(MEM);
  reset_table();
}

static void composite_under_load ()
{
  const int64_t ROWS = 120;
  const int THREADS = 6;
  load(ROWS);
  commit_transaction(MEM);

  Trouble trouble;
  std::atomic<int> wrong(0);
  std::atomic<int> seen(0);

  fan_out(THREADS, [&] (int t) {
      try {
        session();
        for (int n = 0; n < 20; n++) {
          const char* kind = KINDS[(t + n) % KIND_COUNT];
          const std::string kind_s(kind);
          Part::IDX_PART_KIND_WEIGHT.cursor_equal(STRING(kind_s),
            [&] (Zigurat::BTreeIndex<Part, LONG>& level) -> bool {
              level.cursor([&] (Part& row) -> bool {
                  seen++;
                  if ((row.*Part::KIND).to_std_string() != kind_s) wrong++;
                  if ((row.*Part::WEIGHT).value() != (row.*Part::ID).value() * 10) wrong++;
                  return true;
                });
              return true;
            });
        }
        commit_transaction(MEM);
      } catch (std::exception& e) {
        trouble.note(e.what());
      }
    });

  check_str("composite under load: no trouble", trouble.say(), "none");
  check("composite under load: no wrong answers", wrong.load(), 0);
  check("composite under load: the dependent level answered", seen.load() > 0 ? 1 : 0, 1);
  reset_table();
}

static void scan_and_walk_together ()
{
  const int64_t ROWS = 150;
  load(ROWS);
  commit_transaction(MEM);

  Trouble trouble;
  std::atomic<int> wrong(0);
  std::atomic<int> scans(0);
  std::atomic<int> walks(0);

  fan_out(8, [&] (int t) {
      try {
        session();
        for (int n = 0; n < 12; n++) {
          if (t % 2 == 0) {
            long counted = 0;
            Globals::memory()->cursor<Part>([&] (Part& row) -> bool {
                if ((row.*Part::WEIGHT).value() != (row.*Part::ID).value() * 10) wrong++;
                counted++;
                return true;
              });
            if (counted != ROWS) wrong++;
            scans++;
          } else {
            const int64_t want = ((int64_t)n % ROWS) + 1;
            int hits = 0;
            Part::IDX_PART_ID.cursor_equal(LONG(want), [&] (Part& row) -> bool {
                hits++;
                if ((row.*Part::ID).value() != want) wrong++;
                return true;
              });
            if (hits != 1) wrong++;
            walks++;
          }
        }
        commit_transaction(MEM);
      } catch (std::exception& e) {
        trouble.note(e.what());
      }
    });

  check_str("scan beside walk: no trouble", trouble.say(), "none");
  check("scan beside walk: no wrong answers", wrong.load(), 0);
  check("scan beside walk: both made progress", (scans.load() > 0 && walks.load() > 0) ? 1 : 0, 1);
  reset_table();
}

// --- find-then-update: the claim shape --------------------------------------

static void find_then_update ()
{
  const int64_t ROWS = 8;
  const int THREADS = 8;
  const int ROUNDS = 25;
  load(ROWS);
  commit_transaction(MEM);

  Trouble trouble;
  std::atomic<int> done(0);

  fan_out(THREADS, [&] (int t) {
      for (int n = 0; n < ROUNDS; n++) {
        try {
          session(SERIALIZABLE);
          const int64_t want = ((int64_t)(t + n) % ROWS) + 1;
          Part current;
          bool hit = false;
          Part::IDX_PART_ID.cursor_equal(LONG(want), [&] (Part& row) -> bool {
              current = row;
              hit = true;
              return false;
            });
          if (hit) {
            Part next(LONG(want), STRING((current.*Part::KIND).to_std_string()),
                      LONG((current.*Part::WEIGHT).value() + 1));
            Globals::memory()->online_update(current, next);
            done++;
          }
          commit_transaction(MEM);
        } catch (std::exception& e) {
          trouble.note(e.what());
          try { rollback_transaction(MEM); } catch (...) { }
        }
      }
    });

  check_str("claims: no thread met trouble", trouble.say(), "none");
  check("claims: every round claimed", done.load(), THREADS * ROUNDS);

  session();
  long total = 0, rows = 0;
  Globals::memory()->cursor<Part>([&] (Part& row) -> bool {
      total += (row.*Part::WEIGHT).value();
      rows++;
      return true;
    });
  commit_transaction(MEM);

  long opening = 0;
  for (int64_t i = 1; i <= ROWS; i++) opening += i * 10;

  check("claims: the rows all stand", rows, (long)ROWS);
  check("claims: not one increment lost", total, opening + done.load());
  reset_table();
}

// --- a row that is being rewritten ------------------------------------------

static void rewrite_never_missing_from_index ()
{
  load(4);
  commit_transaction(MEM);

  Trouble trouble;
  std::atomic<bool> writing(true);
  std::atomic<int> looks(0);
  std::atomic<int> missing(0);
  std::atomic<int> doubled(0);

  std::thread writer([&] () {
      try {
        for (int n = 0; n < 200; n++) {
          session();
          Part current;
          bool hit = false;
          Part::IDX_PART_ID.cursor_equal(LONG(1), [&] (Part& row) -> bool {
              current = row; hit = true; return false;
            });
          if (hit) {
            Part next(LONG(1), STRING((current.*Part::KIND).to_std_string()),
                      LONG((current.*Part::WEIGHT).value() + 1));
            Globals::memory()->online_update(current, next);
          }
          commit_transaction(MEM);
        }
      } catch (std::exception& e) {
        trouble.note(std::string("writer: ") + e.what());
      }
      writing = false;
    });

  fan_out(4, [&] (int) {
      try {
        while (writing.load()) {
          session();
          int hits = 0;
          Part::IDX_PART_ID.cursor_equal(LONG(1), [&] (Part& row) -> bool {
              hits++;
              if ((row.*Part::ID).value() != 1) missing++;
              return true;
            });
          if (hits == 0) missing++;
          if (hits > 1) doubled++;
          looks++;
          commit_transaction(MEM);
        }
      } catch (std::exception& e) {
        trouble.note(std::string("reader: ") + e.what());
        try { rollback_transaction(MEM); } catch (...) { }
      }
    });

  writer.join();

  check_str("rewrite vs index: no trouble", trouble.say(), "none");
  check("rewrite vs index: readers made progress", looks.load() > 0 ? 1 : 0, 1);
  check("rewrite vs index: the row never vanished", missing.load(), 0);
  check("rewrite vs index: and never doubled", doubled.load(), 0);
  reset_table();
}

static void rewrite_never_missing_from_scan ()
{
  const int64_t ROWS = 6;
  load(ROWS);
  commit_transaction(MEM);

  Trouble trouble;
  std::atomic<bool> writing(true);
  std::atomic<int> scans(0);
  std::atomic<int> short_counts(0);
  std::atomic<int> long_counts(0);

  std::thread writer([&] () {
      try {
        for (int n = 0; n < 200; n++) {
          const int64_t which = (n % ROWS) + 1;
          session();
          Part current;
          bool hit = false;
          Globals::memory()->cursor<Part>([&] (Part& row) -> bool {
              if ((row.*Part::ID).value() == which) { current = row; hit = true; return false; }
              return true;
            });
          if (hit) {
            Part next(LONG(which), STRING((current.*Part::KIND).to_std_string()),
                      LONG((current.*Part::WEIGHT).value() + 1));
            Globals::memory()->online_update(current, next);
          }
          commit_transaction(MEM);
        }
      } catch (std::exception& e) {
        trouble.note(std::string("writer: ") + e.what());
      }
      writing = false;
    });

  fan_out(4, [&] (int) {
      try {
        while (writing.load()) {
          session();
          std::set<int64_t> ids;
          Globals::memory()->cursor<Part>([&] (Part& row) -> bool {
              ids.insert((row.*Part::ID).value());
              return true;
            });
          if ((int64_t)ids.size() < ROWS) short_counts++;
          if ((int64_t)ids.size() > ROWS) long_counts++;
          scans++;
          commit_transaction(MEM);
        }
      } catch (std::exception& e) {
        trouble.note(std::string("reader: ") + e.what());
        try { rollback_transaction(MEM); } catch (...) { }
      }
    });

  writer.join();

  check_str("rewrite vs scan: no trouble", trouble.say(), "none");
  check("rewrite vs scan: scans made progress", scans.load() > 0 ? 1 : 0, 1);
  check("rewrite vs scan: no scan came up short", short_counts.load(), 0);
  check("rewrite vs scan: none came up long", long_counts.load(), 0);
  reset_table();
}

static void scan_counts_exactly_once ()
{
  const int64_t ROWS = 400;
  load(ROWS);
  commit_transaction(MEM);

  Trouble trouble;
  std::atomic<bool> writing(true);
  std::atomic<int> scans(0);
  std::atomic<int> missed(0);
  std::atomic<int> counted_twice(0);

  std::thread writer([&] () {
      try {
        for (int n = 0; n < 400; n++) {
          const int64_t which = (n % ROWS) + 1;
          session();
          Part current;
          bool hit = false;
          Part::IDX_PART_ID.cursor_equal(LONG(which), [&] (Part& row) -> bool {
              current = row; hit = true; return false;
            });
          if (hit) {
            Part next(LONG(which), STRING((current.*Part::KIND).to_std_string()),
                      LONG((current.*Part::WEIGHT).value() + 1));
            Globals::memory()->online_update(current, next);
          }
          commit_transaction(MEM);
        }
      } catch (std::exception& e) {
        trouble.note(std::string("writer: ") + e.what());
      }
      writing = false;
    });

  fan_out(3, [&] (int) {
      try {
        while (writing.load()) {
          session();
          std::vector<int> seen((size_t)ROWS + 1, 0);
          Globals::memory()->cursor<Part>([&] (Part& row) -> bool {
              const int64_t id = (row.*Part::ID).value();
              if (id >= 1 && id <= ROWS) seen[(size_t)id]++;
              return true;
            });
          for (int64_t id = 1; id <= ROWS; id++) {
            if (seen[(size_t)id] == 0) missed++;
            if (seen[(size_t)id] > 1) counted_twice++;
          }
          scans++;
          commit_transaction(MEM);
        }
      } catch (std::exception& e) {
        trouble.note(std::string("reader: ") + e.what());
        try { rollback_transaction(MEM); } catch (...) { }
      }
    });

  writer.join();

  check_str("exactly-once: no trouble", trouble.say(), "none");
  check("exactly-once: scans made progress", scans.load() > 0 ? 1 : 0, 1);
  check("exactly-once: no row missed", missed.load(), 0);
  check("exactly-once: no row counted twice", counted_twice.load(), 0);
  reset_table();
}

static void reader_never_waits ()
{
  load(3);
  commit_transaction(MEM);

  const int original = engine_set_lock_wait_ms(10000);

  std::atomic<bool> staged(false);
  std::atomic<bool> release(false);
  std::atomic<int> hits(0);
  std::atomic<long long> took_ms(0);
  Trouble trouble;

  std::thread writer([&] () {
      try {
        session();
        Part current;
        Globals::memory()->cursor<Part>([&] (Part& row) -> bool {
            if ((row.*Part::ID).value() == 1) { current = row; return false; }
            return true;
          });
        Part next(LONG(1), STRING(std::string("held")), LONG(999));
        Globals::memory()->online_update(current, next);
        staged = true;
        while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        rollback_transaction(MEM);
      } catch (std::exception& e) {
        trouble.note(std::string("writer: ") + e.what());
        staged = true;
      }
    });

  for (int waited = 0; waited < 5000 && !staged.load(); waited += 5)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

  std::thread reader([&] () {
      try {
        const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
        session();
        Globals::memory()->cursor<Part>([&] (Part& row) -> bool {
            if ((row.*Part::ID).value() == 1) {
              hits++;
              if ((row.*Part::WEIGHT).value() != 10) hits += 100;
            }
            return true;
          });
        commit_transaction(MEM);
        took_ms = std::chrono::duration_cast<std::chrono::milliseconds>
          (std::chrono::steady_clock::now() - t0).count();
      } catch (std::exception& e) {
        trouble.note(std::string("reader: ") + e.what());
        try { rollback_transaction(MEM); } catch (...) { }
      }
    });

  reader.join();
  release = true;
  writer.join();
  engine_set_lock_wait_ms(original);

  check_str("no-wait read: no trouble", trouble.say(), "none");
  check("no-wait read: the committed version, once", hits.load(), 1);
  check("no-wait read: well under the lock timeout", took_ms.load() < 1000 ? 1 : 0, 1);
  reset_table();
}

// --- the transaction, not the store -----------------------------------------

static void level_dies_with_its_transaction ()
{
  const IsolationLevel dflt = globals_default_isolation_level();

  begin_transaction(MEM);
  engine_isolate(MEM, REPEATABLE_READ);
  check("a set level shows while the transaction lives", (long)engine_isolation(), (long)REPEATABLE_READ);
  commit_transaction(MEM);

  begin_transaction(MEM);
  check("and the next transaction is the default again", (long)engine_isolation(), (long)dflt);
  engine_isolate(MEM, SNAPSHOT);
  rollback_transaction(MEM);
  begin_transaction(MEM);
  check("a rollback restores it too", (long)engine_isolation(), (long)dflt);
  commit_transaction(MEM);
}

static void serializable_slot_returns ()
{
  Trouble trouble;
  int through = 0;
  for (int i = 0; i < 5; i++) {
    begin_transaction(MEM);
    engine_isolate(MEM, SERIALIZABLE);
    Part row = make_part(900 + i);
    Globals::memory()->online_insert(row);
    commit_transaction(MEM);
    through++;
  }
  check("five serializable commits in a row, one thread", through, 5);

  std::atomic<int> finished(0);
  fan_out(6, [&] (int t) {
      try {
        for (int n = 0; n < 4; n++) {
          begin_transaction(MEM);
          engine_isolate(MEM, SERIALIZABLE);
          Part row = make_part(1000 + t * 10 + n);
          Globals::memory()->online_insert(row);
          commit_transaction(MEM);
          finished++;
        }
      } catch (std::exception& e) {
        trouble.note(e.what());
        try { rollback_transaction(MEM); } catch (...) { }
      }
    });

  check_str("the slot came back every time: no trouble", trouble.say(), "none");
  check("all twenty-four threaded serializable commits", finished.load(), 24);

  session();
  long rows = 0;
  Globals::memory()->cursor<Part>([&] (Part& row) -> bool { rows++; return true; });
  commit_transaction(MEM);
  check("and every row landed", rows, 29);
  reset_table();
}

static void serializable_admits_one ()
{
  std::atomic<int> inside(0);
  std::atomic<int> overlapped(0);
  Trouble trouble;

  fan_out(6, [&] (int t) {
      try {
        for (int n = 0; n < 6; n++) {
          begin_transaction(MEM);
          engine_isolate(MEM, SERIALIZABLE);
          if (inside.fetch_add(1) != 0) overlapped++;
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
          inside--;
          commit_transaction(MEM);
        }
      } catch (std::exception& e) {
        trouble.note(e.what());
        try { rollback_transaction(MEM); } catch (...) { }
      }
    });

  check_str("admits one: no trouble", trouble.say(), "none");
  check("admits one: never two inside", overlapped.load(), 0);
}

static void serializable_wait_gives_up ()
{
  const int original = engine_set_lock_wait_ms(300);

  std::atomic<bool> holding(false);
  std::atomic<bool> release(false);
  std::atomic<int> refused(0);

  std::thread squatter([&] () {
      begin_transaction(MEM);
      engine_isolate(MEM, SERIALIZABLE);
      holding = true;
      while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
      rollback_transaction(MEM);
    });

  for (int waited = 0; waited < 2000 && !holding.load(); waited += 5)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

  std::thread latecomer([&] () {
      try {
        begin_transaction(MEM);
        engine_isolate(MEM, SERIALIZABLE);
        rollback_transaction(MEM);
      } catch (...) {
        refused++;
        try { rollback_transaction(MEM); } catch (...) { }
      }
    });

  latecomer.join();
  release = true;
  squatter.join();
  engine_set_lock_wait_ms(original);

  check("a bounded wait says no rather than hanging", refused.load(), 1);
}

// --- a dead transaction's lock ----------------------------------------------

static void dead_lock_breaks_on_contact ()
{
  const int original = engine_set_lock_wait_ms(10000);

  Trouble trouble;

  session();
  {
    Part seed(LONG(1), STRING(std::string("alive")), LONG(10));
    Globals::memory()->online_insert(seed);
  }
  commit_transaction(MEM);

  std::atomic<bool> staged(false);
  std::atomic<bool> release(false);
  std::thread abandoner([&] () {
      try {
        session();
        Part current;
        bool hit = false;
        Part::IDX_PART_ID.cursor_equal(LONG(1), [&] (Part& row) -> bool {
            current = row; hit = true; return false;
          });
        if (!hit) { trouble.note("abandoner: seed row missing"); staged = true; return; }
        Part doomed(LONG(1), STRING(std::string("doomed")), LONG(11));
        Globals::memory()->online_update(current, doomed);

        engine_retire_transaction(engine_transaction_id(MEM));
        staged = true;
        while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        try { rollback_transaction(MEM); } catch (...) { }
      } catch (std::exception& e) {
        trouble.note(std::string("abandoner: ") + e.what());
        staged = true;
      }
    });
  for (int waited = 0; waited < 5000 && !staged.load(); waited += 5)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

  std::atomic<long long> took_ms(0);
  std::thread writer([&] () {
      try {
        const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
        session();
        Part current;
        bool hit = false;
        Part::IDX_PART_ID.cursor_equal(LONG(1), [&] (Part& row) -> bool {
            current = row; hit = true; return false;
          });
        if (!hit) { trouble.note("writer: row missing"); return; }
        if ((current.*Part::KIND).to_std_string() != "alive")
          trouble.note("writer: read '" + (current.*Part::KIND).to_std_string() +
                       "', the abandoned stage leaked");
        Part fixed(LONG(1), STRING(std::string("fixed")), LONG(12));
        Globals::memory()->online_update(current, fixed);
        commit_transaction(MEM);
        took_ms = 1 + std::chrono::duration_cast<std::chrono::milliseconds>
          (std::chrono::steady_clock::now() - t0).count();
      } catch (std::exception& e) {
        trouble.note(std::string("writer: ") + e.what());
        try { rollback_transaction(MEM); } catch (...) { }
      }
    });
  writer.join();
  release = true;
  abandoner.join();
  engine_set_lock_wait_ms(original);

  check_str("stale lock: no trouble", trouble.say(), "none");
  check("stale lock: through in milliseconds", (took_ms.load() > 0 && took_ms.load() < 1000) ? 1 : 0, 1);

  session();
  Part after;
  bool found = false;
  long rows = 0;
  Globals::memory()->cursor<Part>([&] (Part& row) -> bool {
      rows++;
      if ((row.*Part::ID).value() == 1) { after = row; found = true; }
      return true;
    });
  commit_transaction(MEM);
  check("stale lock: the row survived", found ? 1 : 0, 1);
  check_str("stale lock: the second writer's version stands",
            found ? (after.*Part::KIND).to_std_string() : std::string("gone"), "fixed");
  check("stale lock: and it is the only row", rows, 1);
  reset_table();
}

// ----------------------------------------------------------------------------

int main ()
{
  remove("/tmp/mvccs-contention-hexmap.bin");
  remove("/tmp/mvccs-contention-data.bin");
  Zigurat::filestream h(std::string("/tmp/mvccs-contention-hexmap.bin"),
                        std::ios::in | std::ios::out | std::ios::trunc);
  Zigurat::filestream d(std::string("/tmp/mvccs-contention-data.bin"),
                        std::ios::in | std::ios::out | std::ios::trunc);
  MEM = engine_memory_new();
  memory_open(MEM, (Zigurat::binarystream*)&h, (Zigurat::binarystream*)&d, 8192);
  globals_set_memory(MEM);
  memory_reader_paths(MEM, "/tmp/mvccs-contention-hexmap.bin",
                      "/tmp/mvccs-contention-data.bin");

  concurrent_index_lookups();
  lookups_survive_a_writer();
  every_index_agrees();
  composite_under_load();
  scan_and_walk_together();
  find_then_update();
  rewrite_never_missing_from_index();
  rewrite_never_missing_from_scan();
  scan_counts_exactly_once();
  reader_never_waits();
  level_dies_with_its_transaction();
  serializable_slot_returns();
  serializable_admits_one();
  serializable_wait_gives_up();
  dead_lock_breaks_on_contact();

  printf("\ncontention against libMVCCS.so: %s (%d failures)\n",
         failures == 0 ? "all green" : "RED", failures);
  return failures != 0;
}
