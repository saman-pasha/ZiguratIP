#include "ztest.hpp"
#include "dbfixture.hpp"
#include "transaction.hpp"
#include <thread>
#include <atomic>
#include <chrono>

using namespace Zigurat;


namespace
{
  // Memory::transaction is thread_local, so a second session means a second
  // thread. Every wait is bounded: a test that stalls has to fail, not hang.
  const int WAIT_MS = 8000;

  bool wait_for(std::atomic<int>& flag, int value, int timeout_ms = WAIT_MS)
  {
    for (int waited = 0; waited < timeout_ms; waited += 5) {
      if (flag.load() >= value) return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
  }

  void pause_ms(int ms)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  }

  // A worker session: its own transaction at the requested isolation level.
  void open_session(Memory* memory, IsolationLevel level)
  {
    memory->begin_transaction();
    Memory::transaction.set_isolation_level(level);
  }
}


ZTEST(Isolation, level_round_trips_through_the_transaction)
{
  Store store("iso-level");
  if (!store.ready()) { ZCHECK(false); return; }

  const IsolationLevel original = Memory::transaction.isolation_level();

  Memory::transaction.set_isolation_level(IsolationLevel::READ_UNCOMMITTED);
  ZCHECK(Memory::transaction.isolation_level() == IsolationLevel::READ_UNCOMMITTED);

  Memory::transaction.set_isolation_level(IsolationLevel::REPEATABLE_READ);
  ZCHECK(Memory::transaction.isolation_level() == IsolationLevel::REPEATABLE_READ);

  Memory::transaction.set_isolation_level(IsolationLevel::SNAPSHOT);
  ZCHECK(Memory::transaction.isolation_level() == IsolationLevel::SNAPSHOT);

  Memory::transaction.set_isolation_level(original);
}

// READ UNCOMMITTED is the one level that is allowed to see a dirty row.
ZTEST(Isolation, read_uncommitted_sees_an_uncommitted_insert)
{
  Store store("iso-dirty-read");
  if (!store.ready()) { ZCHECK(false); return; }

  std::atomic<int> writer_phase(0);
  std::atomic<int> reader_phase(0);
  std::atomic<bool> saw_dirty(false);

  std::thread writer([&] () {
      open_session(store.memory, IsolationLevel::READ_COMMITTED);
      Account row(1, "dirty", 100);
      store.memory->online_insert(row);
      writer_phase = 1;                 // written, deliberately not committed

      wait_for(reader_phase, 1);        // let the reader look
      store.memory->rollback_transaction();
      writer_phase = 2;
    });

  std::thread reader([&] () {
      if (!wait_for(writer_phase, 1)) { reader_phase = 1; return; }

      open_session(store.memory, IsolationLevel::READ_UNCOMMITTED);
      Account found;
      saw_dirty = find_by_id(store.memory, 1, found);
      store.memory->rollback_transaction();

      reader_phase = 1;
    });

  reader.join();
  writer.join();

  ZCHECK(saw_dirty.load());
}

// Everything above READ UNCOMMITTED must not hand back a row that was never
// committed. The engine makes the reader wait for the writer to resolve, so the
// row is gone by the time the scan completes.
ZTEST(Isolation, read_committed_never_returns_a_rolled_back_insert)
{
  Store store("iso-no-dirty");
  if (!store.ready()) { ZCHECK(false); return; }

  std::atomic<int> writer_phase(0);
  std::atomic<bool> saw_dirty(false);
  std::atomic<bool> reader_done(false);

  std::thread writer([&] () {
      open_session(store.memory, IsolationLevel::READ_COMMITTED);
      Account row(1, "never-committed", 100);
      store.memory->online_insert(row);
      writer_phase = 1;

      pause_ms(300);                    // hold it open while the reader arrives
      store.memory->rollback_transaction();
      writer_phase = 2;
    });

  std::thread reader([&] () {
      if (!wait_for(writer_phase, 1)) { reader_done = true; return; }

      open_session(store.memory, IsolationLevel::READ_COMMITTED);
      Account found;
      saw_dirty = find_by_id(store.memory, 1, found);
      store.memory->rollback_transaction();
      reader_done = true;
    });

  reader.join();
  writer.join();

  ZCHECK(reader_done.load());
  ZCHECK(!saw_dirty.load());
}

ZTEST(Isolation, read_committed_sees_a_row_once_it_is_committed)
{
  Store store("iso-committed");
  if (!store.ready()) { ZCHECK(false); return; }

  std::atomic<int> writer_phase(0);
  std::atomic<bool> saw_committed(false);

  std::thread writer([&] () {
      open_session(store.memory, IsolationLevel::READ_COMMITTED);
      Account row(1, "committed", 100);
      store.memory->online_insert(row);
      pause_ms(200);
      store.memory->commit_transaction();
      writer_phase = 1;
    });

  std::thread reader([&] () {
      if (!wait_for(writer_phase, 1)) return;

      open_session(store.memory, IsolationLevel::READ_COMMITTED);
      Account found;
      saw_committed = find_by_id(store.memory, 1, found);
      store.memory->rollback_transaction();
    });

  writer.join();
  reader.join();

  ZCHECK(saw_committed.load());
}

ZTEST(Isolation, repeatable_read_does_not_expose_an_uncommitted_insert)
{
  Store store("iso-repeatable");
  if (!store.ready()) { ZCHECK(false); return; }

  std::atomic<int> writer_phase(0);
  std::atomic<bool> saw_dirty(false);
  std::atomic<bool> reader_done(false);

  std::thread writer([&] () {
      open_session(store.memory, IsolationLevel::READ_COMMITTED);
      Account row(1, "uncommitted", 100);
      store.memory->online_insert(row);
      writer_phase = 1;
      pause_ms(300);
      store.memory->rollback_transaction();
      writer_phase = 2;
    });

  std::thread reader([&] () {
      if (!wait_for(writer_phase, 1)) { reader_done = true; return; }

      open_session(store.memory, IsolationLevel::REPEATABLE_READ);
      Account found;
      saw_dirty = find_by_id(store.memory, 1, found);
      store.memory->rollback_transaction();
      reader_done = true;
    });

  reader.join();
  writer.join();

  ZCHECK(reader_done.load());
  ZCHECK(!saw_dirty.load());
}

// A snapshot transaction is pinned to its start time, so a row committed after
// it began must stay invisible to it. transaction.init_time has one second
// resolution, hence the deliberate pause.
ZTEST(Isolation, snapshot_ignores_rows_committed_after_it_started)
{
  Store store("iso-snapshot");
  if (!store.ready()) { ZCHECK(false); return; }

  // A row that predates every snapshot below.
  Account existing(1, "before", 10);
  store.memory->online_insert(existing);
  store.memory->commit_transaction();
  store.memory->begin_transaction();
  pause_ms(1100);

  std::atomic<int> reader_phase(0);
  std::atomic<int> writer_phase(0);
  std::atomic<bool> saw_existing(false);
  std::atomic<bool> saw_later(false);
  std::atomic<size_t> snapshot_count(0);

  std::thread reader([&] () {
      open_session(store.memory, IsolationLevel::SNAPSHOT);
      reader_phase = 1;                       // snapshot taken

      if (!wait_for(writer_phase, 1)) { reader_phase = 2; return; }

      Account found;
      saw_existing = find_by_id(store.memory, 1, found);
      saw_later = find_by_id(store.memory, 2, found);
      snapshot_count = count_rows(store.memory);

      store.memory->rollback_transaction();
      reader_phase = 2;
    });

  std::thread writer([&] () {
      if (!wait_for(reader_phase, 1)) { writer_phase = 1; return; }

      pause_ms(1100);                         // land in a later second
      open_session(store.memory, IsolationLevel::READ_COMMITTED);
      Account later(2, "after", 20);
      store.memory->online_insert(later);
      store.memory->commit_transaction();
      writer_phase = 1;
    });

  reader.join();
  writer.join();

  ZCHECK(saw_existing.load());            // what was there at snapshot time
  ZCHECK(!saw_later.load());              // what arrived afterwards
  ZCHECK_EQ(snapshot_count.load(), (size_t)1);
}

// The row that arrived late is still really there for anyone who looks now.
ZTEST(Isolation, a_later_reader_sees_what_the_snapshot_missed)
{
  Store store("iso-snapshot-after");
  if (!store.ready()) { ZCHECK(false); return; }

  Account first(1, "first", 10);
  store.memory->online_insert(first);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  std::atomic<size_t> later_count(0);

  std::thread writer([&] () {
      open_session(store.memory, IsolationLevel::READ_COMMITTED);
      Account second(2, "second", 20);
      store.memory->online_insert(second);
      store.memory->commit_transaction();
    });
  writer.join();

  std::thread reader([&] () {
      open_session(store.memory, IsolationLevel::READ_COMMITTED);
      later_count = count_rows(store.memory);
      store.memory->rollback_transaction();
    });
  reader.join();

  ZCHECK_EQ(later_count.load(), (size_t)2);
}

// SERIALIZABLE admits one transaction at a time, so overlapping sessions have
// to come out in some serial order rather than interleaving.
ZTEST(Isolation, serializable_transactions_do_not_interleave)
{
  Store store("iso-serializable");
  if (!store.ready()) { ZCHECK(false); return; }

  std::atomic<int> inside(0);
  std::atomic<int> max_inside(0);
  std::atomic<int> completed(0);

  const int SESSIONS = 4;
  std::vector<std::thread> threads;

  for (int t = 0; t < SESSIONS; t++) {
    threads.push_back(std::thread([&, t] () {
	  open_session(store.memory, IsolationLevel::SERIALIZABLE);

	  const int now = ++inside;
	  int previous = max_inside.load();
	  while (now > previous && !max_inside.compare_exchange_weak(previous, now)) { }

	  Account row((int32_t)(t + 1), "serial", t + 1);
	  store.memory->online_insert(row);
	  pause_ms(50);

	  --inside;
	  store.memory->commit_transaction();
	  ++completed;
	}));
  }

  for (size_t i = 0; i < threads.size(); i++) threads[i].join();

  ZCHECK_EQ(completed.load(), SESSIONS);
  ZCHECK_EQ(max_inside.load(), 1);

  store.memory->begin_transaction();
  Memory::transaction.set_isolation_level(IsolationLevel::READ_COMMITTED);
  ZCHECK_EQ(count_rows(store.memory), (size_t)SESSIONS);
}
