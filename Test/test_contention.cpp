// Many threads through the parts of MVCCS that only one ever went through.
//
// WHY THIS IS A SEPARATE SUITE FROM Concurrency. test_concurrency.cpp drives
// Memory's own API -- online_insert, online_update, cursor -- from several
// threads, and those have taken both stream mutexes since they were written. It
// passed throughout. What it never touched is the INDEXES: a table with an
// INDEX KEY does its lookups through BTreeIndex, and BTreeIndex reads the same
// two shared streams. It held no lock at all.
//
// Nothing in the tree noticed, because nothing in the tree used an index from
// two threads at once. A client did. Two connections doing
//
//     SELECT ... WHERE indexed_column == value
//
// at the same moment read from each other's file position, and what came back
// was `hexmap ends inside the chunk at NNNNN' with an address out of nowhere --
// or, often enough, a Pointer assembled from somebody else's offset, a walk
// into it, and the server gone.
//
// So every case here puts more than one thread through an index. They are
// written to fail on the old code: with BTreeIndex unguarded, the first two
// raise inside a few hundred lookups, and `every_index_agrees_with_the_table'
// comes back with rows that were never inserted.
//
// THE OTHER HALF is the transaction itself. A connection holds one Transaction
// for its whole life and initialize() ran once, when the connection was made,
// so anything one transaction changed about itself outlived it -- an isolation
// level most of all, and with it a SERIALIZABLE slot that was taken and never
// given back. The Isolation cases below pin that down.
//
// WHAT THEY LOOK LIKE ON THE OLD CODE, for anyone bisecting: the first, fourth
// and fifth Contention cases raise `hexmap ends inside the chunk at NNNNN' or
// `NULL value' within a few hundred lookups; the sixth and
// `a_serializable_transaction_gives_its_slot_back_when_it_commits' do not fail,
// they HANG -- the slot is never returned and the wait for it had no bound. A
// suite that stops dead partway through the Isolation cases is that bug, not a
// broken test.

#include "ztest.hpp"
#include "dbfixture.hpp"
#include "globals.hpp"
#include "memory.hpp"
#include "transaction.hpp"
#include "btreerecord.hpp"
#include "btreeindex.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace Zigurat;


// --- an indexed table, as compilerddl.cpp would emit it ---------------------

class Part : public BaseTable
{
public:
  using BaseTable::BaseTable;

  static std::string name;
  static hashkey_t hash_key;

  Long   id;
  String kind;
  Long   weight;

  typedef Long   Part::*id_t;
  typedef String Part::*kind_t;
  typedef Long   Part::*weight_t;

  static id_t     ID;
  static kind_t   KIND;
  static weight_t WEIGHT;

  static BTreeIndex<Part, Long>*         IDX_ID;         // unique
  static BTreeIndex<Part, String>*       IDX_KIND;       // not unique
  static BTreeIndex<Part, String, Long>* IDX_KIND_WEIGHT;// composite

  Part() = default;
  Part(int64_t id, const std::string& kind, int64_t weight)
    : id(id), kind(kind), weight(weight) { }

  void prepare() override { }

  void map() override
  {
    if (Part::IDX_ID) Part::IDX_ID->map(*this);
    if (Part::IDX_KIND) Part::IDX_KIND->map(*this);
    if (Part::IDX_KIND_WEIGHT) Part::IDX_KIND_WEIGHT->map(*this);
  }

  void unmap() override
  {
    if (Part::IDX_ID) Part::IDX_ID->unmap(*this);
    if (Part::IDX_KIND) Part::IDX_KIND->unmap(*this);
    if (Part::IDX_KIND_WEIGHT) Part::IDX_KIND_WEIGHT->unmap(*this);
  }

  static void truncate_indexes()
  {
    if (Part::IDX_ID) Part::IDX_ID->truncate();
    if (Part::IDX_KIND) Part::IDX_KIND->truncate();
    if (Part::IDX_KIND_WEIGHT) Part::IDX_KIND_WEIGHT->truncate();
  }

  int64_t pack_size() override
  {
    return binarystream::pack_size(this->id, this->kind, this->weight);
  }

  friend binarystream& operator<<(binarystream& out, const Part& row)
  {
    out.pack(row.id, row.kind, row.weight);
    return out;
  }

  friend binarystream& operator>>(binarystream& in, Part& row)
  {
    in.unpack(row.id, row.kind, row.weight);
    return in;
  }
};

std::string Part::name = "Zigurat::Test::Part";
hashkey_t Part::hash_key = {0x3d, 0x18, 0xc4, 0x77, 0x9a, 0x21, 0x0e, 0x6b, 0xf2, 0x55,
			    0x84, 0xab, 0x39, 0x1c, 0xd7, 0x60, 0x42, 0xe8, 0x0b, 0x93};

Part::id_t     Part::ID     = &Part::id;
Part::kind_t   Part::KIND   = &Part::kind;
Part::weight_t Part::WEIGHT = &Part::weight;

BTreeIndex<Part, Long>*         Part::IDX_ID          = nullptr;
BTreeIndex<Part, String>*       Part::IDX_KIND        = nullptr;
BTreeIndex<Part, String, Long>* Part::IDX_KIND_WEIGHT = nullptr;


namespace
{

  const char* KINDS[] = {"bolt", "nut", "washer", "pin"};
  const int KIND_COUNT = 4;

  // A store with the catalogue bootstrapped and Part's indexes registered, the
  // way load_memory does at server startup.
  class Indexed
  {
  public:
    Store store;
    BTreeIndex<BTreeRecord, String>* catalogue;

    explicit Indexed(const std::string& tag)
      : store(tag), catalogue(nullptr)
    {
      if (!this->store.ready()) return;

      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = nullptr;
      this->catalogue = new BTreeIndex<BTreeRecord, String>
	(this->store.memory, "IDX_ZIGURAT_BTREERECORD_HASH_NAME", true, BTreeRecord::hash_name);
      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = this->catalogue;

      Part::IDX_ID = new BTreeIndex<Part, Long>
	(this->store.memory, "IDX_TEST_PART_ID", true, Part::ID);
      Part::IDX_KIND = new BTreeIndex<Part, String>
	(this->store.memory, "IDX_TEST_PART_KIND", false, Part::KIND);
      Part::IDX_KIND_WEIGHT = new BTreeIndex<Part, String, Long>
	(this->store.memory, "IDX_TEST_PART_KIND_WEIGHT", false, Part::KIND, Part::WEIGHT);

      this->store.memory->commit_transaction();
    }

    bool ready() const { return this->store.ready() && this->catalogue != nullptr; }
    Memory* memory() { return this->store.memory; }

    // Rows 1..n, committed, with a fresh transaction left open.
    void load(int64_t n)
    {
      this->store.memory->begin_transaction();
      for (int64_t i = 1; i <= n; i++) {
	Part row(i, KINDS[i % KIND_COUNT], i * 10);
	this->store.memory->online_insert(row);
      }
      this->store.memory->commit_transaction();
    }

    ~Indexed()
    {
      delete Part::IDX_KIND_WEIGHT; Part::IDX_KIND_WEIGHT = nullptr;
      delete Part::IDX_KIND;        Part::IDX_KIND = nullptr;
      delete Part::IDX_ID;          Part::IDX_ID = nullptr;
      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = nullptr;
      delete this->catalogue;       this->catalogue = nullptr;
    }
  };

  void session(Memory* memory, IsolationLevel level = IsolationLevel::READ_COMMITTED)
  {
    memory->begin_transaction();
    Memory::transaction.set_isolation_level(level);
  }

  void fan_out(int count, std::function<void (int)> body)
  {
    std::vector<std::thread> threads;
    for (int i = 0; i < count; i++) threads.push_back(std::thread(body, i));
    for (size_t i = 0; i < threads.size(); i++) threads[i].join();
  }

  // The first thing that went wrong, remembered rather than described: threads
  // record what they saw go wrong so the failure names the exception instead of
  // just counting one.
  struct Trouble
  {
    std::atomic<int> count;
    std::mutex guard;
    std::string first;

    Trouble() : count(0) { }

    void note(const std::string& what)
    {
      this->count++;
      std::lock_guard<std::mutex> lock(this->guard);
      if (this->first.empty()) this->first = what;
    }

    std::string say() const { return this->first.empty() ? std::string("none") : this->first; }
  };

}


// --- readers over an index --------------------------------------------------

// The plainest form of the bug: nobody writes anything, several threads just
// look rows up by an indexed column. Each walk seeks and reads the shared
// streams, and with no lock between the seek and the read they land on each
// other. No transaction conflicts are possible here, so anything that goes
// wrong is the storage engine and not isolation.
ZTEST(Contention, concurrent_index_lookups_do_not_disturb_each_other)
{
  Indexed db("cont-lookup");
  if (!db.ready()) { ZCHECK(false); return; }

  const int64_t ROWS = 200;
  const int THREADS = 8;
  const int LOOKUPS = 60;
  db.load(ROWS);

  Trouble trouble;
  std::atomic<int> wrong(0);
  std::atomic<int> found(0);

  fan_out(THREADS, [&] (int t) {
      try {
	session(db.memory());
	for (int n = 0; n < LOOKUPS; n++) {
	  const int64_t want = ((int64_t)(t * LOOKUPS + n) % ROWS) + 1;
	  int hits = 0;
	  Part::IDX_ID->cursor_equal(Long(want), [&] (Part& row) -> bool {
	      hits++;
	      // The row the index led to has to be the row the key names.
	      if (row.id.value() != want) wrong++;
	      if (row.weight.value() != want * 10) wrong++;
	      return true;
	    });
	  if (hits != 1) wrong++;
	  found += hits;
	}
	db.memory()->commit_transaction();
      } catch (ZiguratException& e) {
	trouble.note(e.message());
      } catch (std::exception& e) {
	trouble.note(e.what());
      }
    });

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK_EQ(trouble.count.load(), 0);
  ZCHECK_EQ(wrong.load(), 0);
  ZCHECK_EQ(found.load(), THREADS * LOOKUPS);
}

// The same, with the tree being changed underneath. A writer inserting rows
// splits nodes and rewrites keys while the readers are walking them, which is
// where a reader following a stale address ends up somewhere it should not be.
ZTEST(Contention, index_lookups_survive_a_writer_changing_the_tree)
{
  Indexed db("cont-lookup-writer");
  if (!db.ready()) { ZCHECK(false); return; }

  const int64_t ROWS = 150;
  const int READERS = 6;
  db.load(ROWS);

  Trouble trouble;
  std::atomic<bool> writing(true);
  std::atomic<int> wrong(0);
  std::atomic<int> reads(0);

  std::thread writer([&] () {
      try {
	for (int64_t i = ROWS + 1; i <= ROWS + 120; i++) {
	  session(db.memory());
	  Part row(i, KINDS[i % KIND_COUNT], i * 10);
	  db.memory()->online_insert(row);
	  db.memory()->commit_transaction();
	}
      } catch (ZiguratException& e) {
	trouble.note("writer: " + e.message());
      } catch (std::exception& e) {
	trouble.note(std::string("writer: ") + e.what());
      }
      writing = false;
    });

  fan_out(READERS, [&] (int t) {
      try {
	session(db.memory());
	while (writing.load()) {
	  // Only the rows that were there before the writer started: those are
	  // the ones whose answer cannot change, so a wrong answer is wrong
	  // rather than merely early.
	  const int64_t want = ((int64_t)(reads.load() + t) % ROWS) + 1;
	  Part::IDX_ID->cursor_equal(Long(want), [&] (Part& row) -> bool {
	      if (row.id.value() != want) wrong++;
	      if (row.weight.value() != want * 10) wrong++;
	      return true;
	    });
	  reads++;
	}
	db.memory()->commit_transaction();
      } catch (ZiguratException& e) {
	trouble.note("reader: " + e.message());
      } catch (std::exception& e) {
	trouble.note(std::string("reader: ") + e.what());
      }
    });

  writer.join();

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK_EQ(wrong.load(), 0);
  ZCHECK(reads.load() > 0);
}

// Several threads inserting into a table that carries three indexes. Every
// index has to end up describing exactly the rows the table holds -- no entry
// pointing at a row that is not there, and no row missing from an index.
ZTEST(Contention, every_index_agrees_with_the_table_after_parallel_inserts)
{
  Indexed db("cont-agree");
  if (!db.ready()) { ZCHECK(false); return; }

  const int THREADS = 6;
  const int64_t PER_THREAD = 25;
  Trouble trouble;

  fan_out(THREADS, [&] (int t) {
      try {
	session(db.memory());
	for (int64_t i = 0; i < PER_THREAD; i++) {
	  const int64_t id = t * PER_THREAD + i + 1;
	  Part row(id, KINDS[id % KIND_COUNT], id * 10);
	  db.memory()->online_insert(row);
	}
	db.memory()->commit_transaction();
      } catch (ZiguratException& e) {
	trouble.note(e.message());
      } catch (std::exception& e) {
	trouble.note(e.what());
      }
    });

  ZCHECK_STR(trouble.say(), "none");

  const int64_t TOTAL = THREADS * PER_THREAD;

  // What the table says.
  session(db.memory());
  std::set<int64_t> in_table;
  db.memory()->cursor<Part>([&] (Part& row) -> bool {
      in_table.insert(row.id.value());
      return true;
    });
  ZCHECK_EQ((int64_t)in_table.size(), TOTAL);

  // What the unique index says, key by key.
  int missing = 0, duplicated = 0, stray = 0;
  for (int64_t id = 1; id <= TOTAL; id++) {
    int hits = 0;
    Part::IDX_ID->cursor_equal(Long(id), [&] (Part& row) -> bool {
	hits++;
	if (row.id.value() != id) stray++;
	return true;
      });
    if (hits == 0) missing++;
    if (hits > 1) duplicated++;
  }
  ZCHECK_EQ(missing, 0);
  ZCHECK_EQ(duplicated, 0);
  ZCHECK_EQ(stray, 0);

  // And what the non-unique one says: every row of a kind, and only those.
  int64_t by_kind = 0;
  int wrong_kind = 0;
  for (int k = 0; k < KIND_COUNT; k++) {
    Part::IDX_KIND->cursor_equal(String(KINDS[k]), [&] (Part& row) -> bool {
	by_kind++;
	if (row.kind.to_std_string() != KINDS[k]) wrong_kind++;
	if (in_table.find(row.id.value()) == in_table.end()) wrong_kind++;
	return true;
      });
  }
  ZCHECK_EQ(by_kind, TOTAL);
  ZCHECK_EQ(wrong_kind, 0);
  db.memory()->commit_transaction();
}

// A composite index keeps its second column a level down, so a lookup walks two
// trees and holds the streams across both.
ZTEST(Contention, a_composite_index_answers_correctly_under_load)
{
  Indexed db("cont-composite");
  if (!db.ready()) { ZCHECK(false); return; }

  const int64_t ROWS = 120;
  const int THREADS = 6;
  db.load(ROWS);

  Trouble trouble;
  std::atomic<int> wrong(0);
  std::atomic<int> seen(0);

  fan_out(THREADS, [&] (int t) {
      try {
	session(db.memory());
	for (int n = 0; n < 20; n++) {
	  const char* kind = KINDS[(t + n) % KIND_COUNT];
	  Part::IDX_KIND_WEIGHT->cursor_equal(String(kind),
	    [&] (BTreeIndex<Part, Long>& level) -> bool {
	      level.cursor([&] (Part& row) -> bool {
		  seen++;
		  if (row.kind.to_std_string() != kind) wrong++;
		  if (row.weight.value() != row.id.value() * 10) wrong++;
		  return true;
		});
	      return true;
	    });
	}
	db.memory()->commit_transaction();
      } catch (ZiguratException& e) {
	trouble.note(e.message());
      } catch (std::exception& e) {
	trouble.note(e.what());
      }
    });

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK_EQ(wrong.load(), 0);
  ZCHECK(seen.load() > 0);
}

// A table scan and an index walk read the same two streams by different routes.
// Running both at once is what tells the two guards apart: if either side is
// unguarded the other's file position is what it reads from.
ZTEST(Contention, a_table_scan_and_an_index_walk_can_run_together)
{
  Indexed db("cont-scan-and-walk");
  if (!db.ready()) { ZCHECK(false); return; }

  const int64_t ROWS = 150;
  db.load(ROWS);

  Trouble trouble;
  std::atomic<int> wrong(0);
  std::atomic<int> scans(0);
  std::atomic<int> walks(0);

  fan_out(8, [&] (int t) {
      try {
	session(db.memory());
	for (int n = 0; n < 12; n++) {
	  if (t % 2 == 0) {
	    int64_t counted = 0;
	    db.memory()->cursor<Part>([&] (Part& row) -> bool {
		if (row.weight.value() != row.id.value() * 10) wrong++;
		counted++;
		return true;
	      });
	    if (counted != ROWS) wrong++;
	    scans++;
	  } else {
	    const int64_t want = ((int64_t)n % ROWS) + 1;
	    int hits = 0;
	    Part::IDX_ID->cursor_equal(Long(want), [&] (Part& row) -> bool {
		hits++;
		if (row.id.value() != want) wrong++;
		return true;
	      });
	    if (hits != 1) wrong++;
	    walks++;
	  }
	}
	db.memory()->commit_transaction();
      } catch (ZiguratException& e) {
	trouble.note(e.message());
      } catch (std::exception& e) {
	trouble.note(e.what());
      }
    });

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK_EQ(wrong.load(), 0);
  ZCHECK(scans.load() > 0);
  ZCHECK(walks.load() > 0);
}

// The shape a worker pool takes: find a row through an index, change it, commit,
// and go round again. It is what cocolog's machine_claim_named does, which is
// the workload that found all of this.
ZTEST(Contention, find_by_index_then_update_holds_up_from_many_threads)
{
  Indexed db("cont-find-update");
  if (!db.ready()) { ZCHECK(false); return; }

  const int64_t ROWS = 8;
  const int THREADS = 8;
  const int ROUNDS = 25;
  db.load(ROWS);

  Trouble trouble;
  std::atomic<int> done(0);

  fan_out(THREADS, [&] (int t) {
      for (int n = 0; n < ROUNDS; n++) {
	try {
	  // SERIALIZABLE for the read-then-write, the way a claim has to be:
	  // at READ COMMITTED two threads read the same weight and one of the
	  // two increments is lost.
	  session(db.memory(), IsolationLevel::SERIALIZABLE);

	  const int64_t want = ((int64_t)(t + n) % ROWS) + 1;
	  Part current;
	  bool hit = false;
	  Part::IDX_ID->cursor_equal(Long(want), [&] (Part& row) -> bool {
	      current = row;
	      hit = true;
	      return false;
	    });

	  if (hit) {
	    Part next(want, current.kind.to_std_string(), current.weight.value() + 1);
	    db.memory()->online_update(current, next);
	    done++;
	  }
	  db.memory()->commit_transaction();
	} catch (ZiguratException& e) {
	  trouble.note(e.message());
	  try { db.memory()->rollback_transaction(); } catch (...) { }
	} catch (std::exception& e) {
	  trouble.note(e.what());
	  try { db.memory()->rollback_transaction(); } catch (...) { }
	}
      }
    });

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK_EQ(done.load(), THREADS * ROUNDS);

  // Every increment landed: the weights have to add up to what was there plus
  // one per completed round. A lost update shows here and nowhere else.
  session(db.memory());
  int64_t total = 0;
  int64_t rows = 0;
  db.memory()->cursor<Part>([&] (Part& row) -> bool {
      total += row.weight.value();
      rows++;
      return true;
    });
  db.memory()->commit_transaction();

  int64_t opening = 0;
  for (int64_t i = 1; i <= ROWS; i++) opening += i * 10;

  ZCHECK_EQ(rows, ROWS);
  ZCHECK_EQ(total, opening + done.load());
}


// --- a row that is being rewritten ------------------------------------------

// READ COMMITTED promises the last committed version of a row. Not a newer one,
// not a half-written one -- but not NOTHING either, and nothing is what a reader
// used to get for the whole length of somebody else's update.
//
// An update writes the new version to a NEW address and marks the old one
// UPDATED and locked. A reader arriving at the old one waited for that lock, and
// by the time the wait ended the old version had been retired and the new one
// was somewhere the reader had already been. Both versions gone, from a
// transaction that had done nothing but read.
//
// This is the shape a queue poller takes -- "is the row still there?" -- and it
// is where it was found: a worker asking after a job its partner happened to be
// saving was told the job did not exist.
ZTEST(Contention, a_row_being_updated_is_never_missing_from_an_index)
{
  Indexed db("cont-rewrite-index");
  if (!db.ready()) { ZCHECK(false); return; }

  db.load(4);

  Trouble trouble;
  std::atomic<bool> writing(true);
  std::atomic<int> looks(0);
  std::atomic<int> missing(0);
  std::atomic<int> doubled(0);

  // One row rewritten over and over. Its id never changes, so every lookup of
  // that id has exactly one answer at every instant.
  std::thread writer([&] () {
      try {
	for (int n = 0; n < 200; n++) {
	  session(db.memory());
	  Part current;
	  bool hit = false;
	  Part::IDX_ID->cursor_equal(Long(1), [&] (Part& row) -> bool {
	      current = row; hit = true; return false;
	    });
	  if (hit) {
	    Part next(1, current.kind.to_std_string(), current.weight.value() + 1);
	    db.memory()->online_update(current, next);
	  }
	  db.memory()->commit_transaction();
	}
      } catch (ZiguratException& e) {
	trouble.note("writer: " + e.message());
      }
      writing = false;
    });

  fan_out(4, [&] (int) {
      try {
	while (writing.load()) {
	  session(db.memory());
	  int hits = 0;
	  Part::IDX_ID->cursor_equal(Long(1), [&] (Part& row) -> bool {
	      hits++;
	      // Whichever version it is, it has to be a whole one.
	      if (row.id.value() != 1) missing++;
	      return true;
	    });
	  if (hits == 0) missing++;
	  if (hits > 1) doubled++;
	  looks++;
	  db.memory()->commit_transaction();
	}
      } catch (ZiguratException& e) {
	trouble.note("reader: " + e.message());
	try { db.memory()->rollback_transaction(); } catch (...) { }
      }
    });

  writer.join();

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK(looks.load() > 0);
  ZCHECK_EQ(missing.load(), 0);
  ZCHECK_EQ(doubled.load(), 0);
}

// The same thing through a table scan rather than an index. A scan visits
// addresses in order and an update puts the new version wherever the allocator
// has room -- which can be somewhere the scan has already been -- so this is the
// harder half.
ZTEST(Contention, a_row_being_updated_is_never_missing_from_a_scan)
{
  Indexed db("cont-rewrite-scan");
  if (!db.ready()) { ZCHECK(false); return; }

  const int64_t ROWS = 6;
  db.load(ROWS);

  Trouble trouble;
  std::atomic<bool> writing(true);
  std::atomic<int> scans(0);
  std::atomic<int> short_counts(0);
  std::atomic<int> long_counts(0);

  std::thread writer([&] () {
      try {
	for (int n = 0; n < 200; n++) {
	  const int64_t which = (n % ROWS) + 1;
	  session(db.memory());
	  Part current;
	  bool hit = false;
	  db.memory()->cursor<Part>([&] (Part& row) -> bool {
	      if (row.id.value() == which) { current = row; hit = true; return false; }
	      return true;
	    });
	  if (hit) {
	    Part next(which, current.kind.to_std_string(), current.weight.value() + 1);
	    db.memory()->online_update(current, next);
	  }
	  db.memory()->commit_transaction();
	}
      } catch (ZiguratException& e) {
	trouble.note("writer: " + e.message());
      }
      writing = false;
    });

  fan_out(4, [&] (int) {
      try {
	while (writing.load()) {
	  session(db.memory());
	  std::set<int64_t> ids;
	  db.memory()->cursor<Part>([&] (Part& row) -> bool {
	      ids.insert(row.id.value());
	      return true;
	    });
	  if ((int64_t)ids.size() < ROWS) short_counts++;
	  if ((int64_t)ids.size() > ROWS) long_counts++;
	  scans++;
	  db.memory()->commit_transaction();
	}
      } catch (ZiguratException& e) {
	trouble.note("reader: " + e.message());
	try { db.memory()->rollback_transaction(); } catch (...) { }
      }
    });

  writer.join();

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK(scans.load() > 0);
  ZCHECK_EQ(short_counts.load(), 0);
  ZCHECK_EQ(long_counts.load(), 0);
}

// A SCAN SPANS TIME, and that is the harder half of the same problem. It visits
// addresses in order; an update puts the replacement wherever the allocator has
// room. So if the writer commits in the middle of a scan, the scan can have
// passed the replacement while it was uncommitted and then find the original
// retired -- the row missing -- or seen the original and then reach the
// committed replacement, and count the same row twice.
//
// The case above only catches this when the window happens to line up. This one
// makes it certain: enough rows that a scan takes long enough for commits to
// land inside it, and a writer doing nothing else.
//
// EXACTLY ONCE is the whole assertion. Which version a scan sees is its own
// business -- the row may be mid-rewrite and either answer is a real one -- but
// a row that exists throughout has to be counted once.
ZTEST(Contention, a_scan_counts_every_row_exactly_once_while_a_writer_commits)
{
  Indexed db("cont-scan-snapshot");
  if (!db.ready()) { ZCHECK(false); return; }

  const int64_t ROWS = 400;
  db.load(ROWS);

  Trouble trouble;
  std::atomic<bool> writing(true);
  std::atomic<int> scans(0);
  std::atomic<int> missed(0);
  std::atomic<int> counted_twice(0);

  std::thread writer([&] () {
      try {
	for (int n = 0; n < 400; n++) {
	  const int64_t which = (n % ROWS) + 1;
	  session(db.memory());
	  Part current;
	  bool hit = false;
	  Part::IDX_ID->cursor_equal(Long(which), [&] (Part& row) -> bool {
	      current = row; hit = true; return false;
	    });
	  if (hit) {
	    Part next(which, current.kind.to_std_string(), current.weight.value() + 1);
	    db.memory()->online_update(current, next);
	  }
	  db.memory()->commit_transaction();
	}
      } catch (ZiguratException& e) {
	trouble.note("writer: " + e.message());
      }
      writing = false;
    });

  fan_out(3, [&] (int) {
      try {
	while (writing.load()) {
	  session(db.memory());
	  std::vector<int> seen((size_t)ROWS + 1, 0);
	  db.memory()->cursor<Part>([&] (Part& row) -> bool {
	      const int64_t id = row.id.value();
	      if (id >= 1 && id <= ROWS) seen[(size_t)id]++;
	      return true;
	    });
	  for (int64_t id = 1; id <= ROWS; id++) {
	    if (seen[(size_t)id] == 0) missed++;
	    if (seen[(size_t)id] > 1) counted_twice++;
	  }
	  scans++;
	  db.memory()->commit_transaction();
	}
      } catch (ZiguratException& e) {
	trouble.note("reader: " + e.message());
	try { db.memory()->rollback_transaction(); } catch (...) { }
      }
    });

  writer.join();

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK(scans.load() > 0);
  ZCHECK_EQ(missed.load(), 0);
  ZCHECK_EQ(counted_twice.load(), 0);
}

// A reader must not be made to wait by a writer at all at this level. Waiting is
// what turned one slow update into every reader's problem, and it is not what
// READ COMMITTED asks for: the committed version is sitting at the address the
// reader is already looking at.
ZTEST(Contention, a_reader_does_not_wait_for_a_writer_at_read_committed)
{
  Indexed db("cont-nowait");
  if (!db.ready()) { ZCHECK(false); return; }

  db.load(3);

  const int original = Memory::lock_wait_timeout_ms;
  Memory::lock_wait_timeout_ms = 10000;

  std::atomic<bool> staged(false);
  std::atomic<bool> release(false);
  std::atomic<int> hits(0);
  std::atomic<long long> took_ms(0);
  Trouble trouble;

  // A writer that stages an update and sits on it.
  std::thread writer([&] () {
      try {
	session(db.memory());
	Part current;
	db.memory()->cursor<Part>([&] (Part& row) -> bool {
	    if (row.id.value() == 1) { current = row; return false; }
	    return true;
	  });
	Part next(1, "held", 999);
	db.memory()->online_update(current, next);
	staged = true;
	while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
	db.memory()->rollback_transaction();
      } catch (ZiguratException& e) {
	trouble.note("writer: " + e.message());
      }
    });

  for (int waited = 0; waited < 5000 && !staged.load(); waited += 5)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

  std::thread reader([&] () {
      try {
	const std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
	session(db.memory());
	db.memory()->cursor<Part>([&] (Part& row) -> bool {
	    if (row.id.value() == 1) {
	      hits++;
	      // The committed version, not the one being staged.
	      if (row.weight.value() != 10) hits += 100;
	    }
	    return true;
	  });
	db.memory()->commit_transaction();
	took_ms = std::chrono::duration_cast<std::chrono::milliseconds>
	  (std::chrono::steady_clock::now() - t0).count();
      } catch (ZiguratException& e) {
	trouble.note("reader: " + e.message());
	try { db.memory()->rollback_transaction(); } catch (...) { }
      }
    });

  reader.join();
  release = true;
  writer.join();
  Memory::lock_wait_timeout_ms = original;

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK_EQ(hits.load(), 1);
  // Well under the lock wait timeout: it did not queue behind the writer.
  ZCHECK(took_ms.load() < 1000);
}


// --- the transaction, not the store -----------------------------------------

// A level one transaction asked for must not be the next transaction's level.
// It was: a connection holds one Transaction for its whole life and only
// initialize() reset it, so whatever a procedure set stayed set until the client
// hung up.
ZTEST(Isolation, a_level_does_not_outlive_the_transaction_that_set_it)
{
  Store store("iso-leak");
  if (!store.ready()) { ZCHECK(false); return; }

  const IsolationLevel dflt = Globals::default_isolation_level();

  store.memory->begin_transaction();
  Memory::transaction.set_isolation_level(IsolationLevel::REPEATABLE_READ);
  ZCHECK(Memory::transaction.isolation_level() == IsolationLevel::REPEATABLE_READ);
  store.memory->commit_transaction();

  // The next transaction on this connection is the connection's level again.
  ZCHECK(Memory::transaction.isolation_level() == dflt);

  store.memory->begin_transaction();
  Memory::transaction.set_isolation_level(IsolationLevel::SNAPSHOT);
  store.memory->rollback_transaction();
  ZCHECK(Memory::transaction.isolation_level() == dflt);
}

// And the slot that SERIALIZABLE takes has to come back with it. This is the
// case that hung: one thread took the slot, committed, and kept it, so the
// second thread waited on a semaphore whose owner had finished with it minutes
// ago. From outside it looked like one client doing all the work.
ZTEST(Isolation, a_serializable_transaction_gives_its_slot_back_when_it_commits)
{
  Store store("iso-slot");
  if (!store.ready()) { ZCHECK(false); return; }

  std::atomic<int> through(0);
  Trouble trouble;

  // Five in a row on one thread: without the release each would wait for the
  // one before it, which never ends.
  for (int i = 0; i < 5; i++) {
    store.memory->begin_transaction();
    Memory::transaction.set_isolation_level(IsolationLevel::SERIALIZABLE);
    Account row((int32_t)(i + 1), "serial", i);
    store.memory->online_insert(row);
    store.memory->commit_transaction();
    through++;
  }
  ZCHECK_EQ(through.load(), 5);

  // And from several threads, which is where it has to queue rather than
  // deadlock. Each takes the slot, does its work and hands it on.
  std::atomic<int> finished(0);
  fan_out(6, [&] (int t) {
      try {
	for (int n = 0; n < 4; n++) {
	  store.memory->begin_transaction();
	  Memory::transaction.set_isolation_level(IsolationLevel::SERIALIZABLE);
	  Account row((int32_t)(100 + t * 10 + n), "serial", n);
	  store.memory->online_insert(row);
	  store.memory->commit_transaction();
	  finished++;
	}
      } catch (ZiguratException& e) {
	trouble.note(e.message());
	try { store.memory->rollback_transaction(); } catch (...) { }
      } catch (std::exception& e) {
	trouble.note(e.what());
	try { store.memory->rollback_transaction(); } catch (...) { }
      }
    });

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK_EQ(finished.load(), 24);

  session(store.memory);
  ZCHECK_EQ(count_rows(store.memory), (size_t)29);
  store.memory->commit_transaction();
}

// Only one at a time, though -- that is what the level means. Two threads that
// both take it must not be inside it together.
ZTEST(Isolation, serializable_admits_one_transaction_at_a_time)
{
  Store store("iso-exclusive");
  if (!store.ready()) { ZCHECK(false); return; }

  std::atomic<int> inside(0);
  std::atomic<int> overlapped(0);
  Trouble trouble;

  fan_out(6, [&] (int t) {
      try {
	for (int n = 0; n < 6; n++) {
	  store.memory->begin_transaction();
	  Memory::transaction.set_isolation_level(IsolationLevel::SERIALIZABLE);

	  if (inside.fetch_add(1) != 0) overlapped++;
	  std::this_thread::sleep_for(std::chrono::milliseconds(2));
	  inside--;

	  store.memory->commit_transaction();
	}
      } catch (ZiguratException& e) {
	trouble.note(e.message());
	try { store.memory->rollback_transaction(); } catch (...) { }
      }
    });

  ZCHECK_STR(trouble.say(), "none");
  ZCHECK_EQ(overlapped.load(), 0);
}

// A client is entitled to take the slot and then say nothing. That used to be
// every other connection's problem for ever; now it is a wait that ends.
ZTEST(Isolation, a_serializable_wait_gives_up_rather_than_hanging)
{
  Store store("iso-timeout");
  if (!store.ready()) { ZCHECK(false); return; }

  const int original = Memory::lock_wait_timeout_ms;
  Memory::lock_wait_timeout_ms = 300;

  std::atomic<bool> holding(false);
  std::atomic<bool> release(false);
  std::atomic<int> refused(0);

  // One thread takes the slot and sits on it without committing.
  std::thread squatter([&] () {
      store.memory->begin_transaction();
      Memory::transaction.set_isolation_level(IsolationLevel::SERIALIZABLE);
      holding = true;
      while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
      store.memory->rollback_transaction();
    });

  for (int waited = 0; waited < 2000 && !holding.load(); waited += 5)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

  std::thread latecomer([&] () {
      try {
	store.memory->begin_transaction();
	Memory::transaction.set_isolation_level(IsolationLevel::SERIALIZABLE);
	store.memory->rollback_transaction();
      } catch (ZiguratException&) {
	refused++;
      }
    });

  latecomer.join();
  release = true;
  squatter.join();

  Memory::lock_wait_timeout_ms = original;

  // It came back at all, and it came back saying no rather than saying yes.
  ZCHECK_EQ(refused.load(), 1);
}
