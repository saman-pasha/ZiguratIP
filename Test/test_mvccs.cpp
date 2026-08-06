#include "ztest.hpp"
#include "dbfixture.hpp"
#include "globals.hpp"
#include "memory.hpp"
#include "btreerecord.hpp"
#include "btreeindex.hpp"
#include "isolationlevel.hpp"
#include "filestream.hpp"
#include "bufferstream.hpp"
#include <cstdio>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

using namespace Zigurat;


ZTEST(MVCCS, page_store_initialises)
{
  Store store("mvccs-init");
  ZCHECK(store.hexmap.good());
  ZCHECK(store.data.good());
  ZCHECK(store.ready());
}

ZTEST(MVCCS, transaction_lifecycle)
{
  Store store("mvccs-txn");
  if (!store.ready()) { ZCHECK(false); return; }

  ZCHECK_NOTHROW(store.memory->begin_transaction());
  const size_t first = store.memory->transaction_id();
  ZCHECK_NOTHROW(store.memory->commit_transaction());

  ZCHECK_NOTHROW(store.memory->begin_transaction());
  const size_t second = store.memory->transaction_id();
  ZCHECK_NOTHROW(store.memory->rollback_transaction());

  // Every transaction gets its own identity.
  ZCHECK(first != 0);
  ZCHECK(second != 0);
}

ZTEST(MVCCS, commit_persists_pages_to_the_files)
{
  Store store("mvccs-persist");
  if (!store.ready()) { ZCHECK(false); return; }

  store.memory->commit_transaction();
  store.hexmap.flush();
  store.data.flush();

  // Initialisation plus a commit must have written the control structures.
  ZCHECK(store.hexmap.length() > 0);
  ZCHECK(store.data.length() > 0);
}

ZTEST(MVCCS, dba_reports_describe_the_store)
{
  Store store("mvccs-dba");
  if (!store.ready()) { ZCHECK(false); return; }

  bufferstream pagefiles;
  ZCHECK_NOTHROW(store.memory->dba_pagefiles(pagefiles));
  ZCHECK(pagefiles.string().size() > 0);

  bufferstream pointers;
  ZCHECK_NOTHROW(store.memory->dba_pointers(pointers));
}

// The catalogue index bootstraps itself by scanning the INDICES pages; every
// other index is then looked up through it. This is the order load_memory uses.
ZTEST(MVCCS, catalogue_index_bootstraps_then_carries_the_others)
{
  Store store("mvccs-btree");
  if (!store.ready()) { ZCHECK(false); return; }

  typedef BTreeIndex<BTreeRecord, String> NameIndex;   // the comma would split the macro

  NameIndex* catalogue = nullptr;
  ZCHECK_NOTHROW(catalogue = new NameIndex(store.memory, "IDX_ZIGURAT_BTREERECORD_HASH_NAME",
					   true, BTreeRecord::hash_name));
  ZCHECK(catalogue != nullptr);

  BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = catalogue;
  ZCHECK_NOTHROW(store.memory->commit_transaction());

  // With the catalogue in place a secondary index can register itself.
  NameIndex* secondary = nullptr;
  ZCHECK_NOTHROW(secondary = new NameIndex(store.memory, "IDX_TEST_SECONDARY",
					   false, BTreeRecord::hash_name));
  ZCHECK(secondary != nullptr);
  ZCHECK_NOTHROW(store.memory->commit_transaction());

  BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = nullptr;
  delete secondary;
  delete catalogue;
}

// Without the catalogue, a secondary index used to dereference a null static.
ZTEST(MVCCS, a_secondary_index_without_the_catalogue_is_rejected)
{
  Store store("mvccs-btree-nocat");
  if (!store.ready()) { ZCHECK(false); return; }

  typedef BTreeIndex<BTreeRecord, String> NameIndex;

  BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = nullptr;
  ZCHECK_THROWS(new NameIndex(store.memory, "IDX_TEST_ORPHAN", false, BTreeRecord::hash_name));
}

ZTEST(MVCCS, isolation_level_defaults_round_trip_through_globals)
{
  const IsolationLevel original = Globals::default_isolation_level();

  Globals::set_default_isolation_level(IsolationLevel::SERIALIZABLE);
  ZCHECK(Globals::default_isolation_level() == IsolationLevel::SERIALIZABLE);

  Globals::set_default_isolation_level(IsolationLevel::READ_COMMITTED);
  ZCHECK(Globals::default_isolation_level() == IsolationLevel::READ_COMMITTED);

  Globals::set_default_isolation_level(original);
}

ZTEST(MVCCS, autocommit_mode_round_trips_through_globals)
{
  const bool original = Globals::default_autocommit_mode();

  Globals::set_default_autocommit_mode(true);
  ZCHECK(Globals::default_autocommit_mode());

  Globals::set_default_autocommit_mode(false);
  ZCHECK(!Globals::default_autocommit_mode());

  Globals::set_default_autocommit_mode(original);
}

// Compiled code writes its results straight through the client stream and never
// tests it first, so an unbound one used to be a call through a vtable at
// address zero: the process died inside the user's procedure with a stack that
// explained nothing. Asking for a stream that is not there has to be an error
// the connection can be told about.
ZTEST(MVCCS, an_unbound_client_stream_is_an_error_and_not_a_null_pointer)
{
  Zigurat::binarystream* const original = []() -> Zigurat::binarystream* {
    try { return Globals::client_stream(); } catch (const ZiguratException&) { return nullptr; }
  }();

  Globals::set_client_stream(nullptr);
  ZCHECK_THROWS(Globals::client_stream());

  // The echo stream keeps answering null, because generated code tests it to
  // decide whether it is serving a page or a client.
  Zigurat::textstream* const echo = Globals::echo_stream();
  Globals::set_echo_stream(nullptr);
  ZCHECK(Globals::echo_stream() == nullptr);
  Globals::set_echo_stream(echo);

  Globals::set_client_stream(original);
}


// ---------------------------------------------------------------------------
// Startup reconstructs the free list from the hexmap
// ---------------------------------------------------------------------------

// A freed record with a live one after it in the same page.
//
// Memory::_initialize walks each page from its first chunk and used to STOP at
// the first chunk that was not data, registering that one free run and
// abandoning the rest of the page. That is only right if free space is always
// the page's tail, and it is not: a rolled-back transaction frees its record
// wherever it happens to sit, and anything allocated after it is then behind a
// hole.
//
// Two threads are what it takes to build that shape, because Memory::transaction
// is thread_local and one thread cannot hold two. A begins, B begins after it,
// A rolls back -- so the hole is A's record and B's is the live thing behind it.
//
// What went wrong was not the abandoning by itself. _pointer measures a record
// by skipping CONTROL_COUNT control chunks and reading to the standalone bit,
// and a free run has no control block -- so on a short hole it reads straight
// past the hole into the record behind it and answers a size that covers both.
// That entry goes into the free list, and the next allocation from it writes
// over a live row.
ZTEST(MVCCS, a_hole_in_a_page_does_not_swallow_the_record_behind_it)
{
  const std::string TAG = "holewalk";
  int64_t survivor = 0;

  {
    Store store(TAG);
    if (!store.ready()) { ZCHECK(false); return; }

    std::mutex m;
    std::condition_variable cv;
    int step = 0;

    // A takes its transaction record first, B takes the next one, then A frees
    // its own -- leaving B's behind the hole.
    std::thread a([&] {
	store.memory->begin_transaction();
	{ std::lock_guard<std::mutex> lk(m); step = 1; }
	cv.notify_all();
	{ std::unique_lock<std::mutex> lk(m); cv.wait(lk, [&] { return step >= 2; }); }
	store.memory->rollback_transaction();
	{ std::lock_guard<std::mutex> lk(m); step = 3; }
	cv.notify_all();
      });

    std::thread b([&] {
	{ std::unique_lock<std::mutex> lk(m); cv.wait(lk, [&] { return step >= 1; }); }
	store.memory->begin_transaction();
	{ std::lock_guard<std::mutex> lk(m); step = 2; }
	cv.notify_all();
	{ std::unique_lock<std::mutex> lk(m); cv.wait(lk, [&] { return step >= 3; }); }
	Account behind(7, "behind the hole", 4242);
	store.memory->online_insert(behind);
	store.memory->commit_transaction();
      });

    a.join();
    b.join();

    std::vector<Account> rows = select_all(store.memory);
    ZCHECK_EQ(rows.size(), (size_t)1);
    if (!rows.empty()) survivor = rows[0].balance.value();
    ZCHECK_EQ(survivor, (int64_t)4242);

    store.detach();

    // Keep the files: ~Store erases whatever paths it still holds, and the
    // whole point here is to open them again.
    store.hexmap_path.clear();
    store.data_path.clear();
  }

  // Reopen: this is where _initialize rebuilds the free list from the hexmap.
  {
    Store reopened(TAG, false);
    if (!reopened.ready()) { ZCHECK(false); return; }

    std::vector<Account> before = select_all(reopened.memory);
    ZCHECK_EQ(before.size(), (size_t)1);

    // And allocate, which is what turns a wrong free list into a lost row: the
    // entry that covered the hole covered the record behind it too.
    reopened.memory->begin_transaction();
    Account fresh(8, "allocated after the restart", 99);
    reopened.memory->online_insert(fresh);
    reopened.memory->commit_transaction();

    std::vector<Account> after = select_all(reopened.memory);
    ZCHECK_EQ(after.size(), (size_t)2);

    bool survived = false;
    for (size_t i = 0; i < after.size(); i++) {
      if (after[i].id == 7) survived = (after[i].balance == 4242 &&
				       after[i].owner == "behind the hole");
    }
    ZCHECK(survived);

    reopened.erase();
  }
}
