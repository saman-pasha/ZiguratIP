#include "ztest.h"
#include "dbfixture.h"
#include "transaction.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <algorithm>

using namespace Zigurat;


namespace
{
  void session(Memory* memory, IsolationLevel level = IsolationLevel::READ_COMMITTED)
  {
    memory->begin_transaction();
    Memory::transaction.set_isolation_level(level);
  }

  // Runs body(index) on `count` threads and waits for all of them.
  void fan_out(int count, std::function<void (int)> body)
  {
    std::vector<std::thread> threads;
    for (int i = 0; i < count; i++) threads.push_back(std::thread(body, i));
    for (size_t i = 0; i < threads.size(); i++) threads[i].join();
  }
}


ZTEST(Concurrency, parallel_inserts_all_land_exactly_once)
{
  Store store("conc-insert");
  if (!store.ready()) { ZCHECK(false); return; }

  const int THREADS = 6;
  const int PER_THREAD = 20;
  std::atomic<int> failures(0);

  fan_out(THREADS, [&] (int t) {
      try {
	session(store.memory);
	for (int i = 0; i < PER_THREAD; i++) {
	  Account row((int32_t)(t * PER_THREAD + i + 1), "worker", 1);
	  store.memory->online_insert(row);
	}
	store.memory->commit_transaction();
      } catch (...) {
	failures++;
      }
    });

  ZCHECK_EQ(failures.load(), 0);

  session(store.memory);
  std::vector<Account> rows = select_all(store.memory);
  ZCHECK_EQ(rows.size(), (size_t)(THREADS * PER_THREAD));

  // Every id exactly once: no lost writes, no duplicated pages.
  std::vector<int32_t> ids;
  for (size_t i = 0; i < rows.size(); i++) ids.push_back(rows[i].id.value());
  std::sort(ids.begin(), ids.end());
  ZCHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());

  bool contiguous = true;
  for (size_t i = 0; i < ids.size(); i++)
    if (ids[i] != (int32_t)(i + 1)) contiguous = false;
  ZCHECK(contiguous);
}

ZTEST(Concurrency, parallel_rollbacks_leave_nothing_behind)
{
  Store store("conc-rollback");
  if (!store.ready()) { ZCHECK(false); return; }

  const int THREADS = 6;

  // One committed row per thread, then a rolled back batch on top.
  fan_out(THREADS, [&] (int t) {
      session(store.memory);
      Account keeper((int32_t)(t + 1), "keeper", 1);
      store.memory->online_insert(keeper);
      store.memory->commit_transaction();

      session(store.memory);
      for (int i = 0; i < 10; i++) {
	Account ghost((int32_t)(1000 + t * 10 + i), "ghost", 1);
	store.memory->online_insert(ghost);
      }
      store.memory->rollback_transaction();
    });

  session(store.memory);
  std::vector<Account> rows = select_all(store.memory);
  ZCHECK_EQ(rows.size(), (size_t)THREADS);

  bool ghost_found = false;
  for (size_t i = 0; i < rows.size(); i++)
    if (rows[i].id.value() >= 1000) ghost_found = true;
  ZCHECK(!ghost_found);
}

// The classic invariant: money moves between accounts but never appears or
// disappears, no matter how the transfers interleave.
ZTEST(Concurrency, parallel_transfers_conserve_the_total)
{
  Store store("conc-transfer");
  if (!store.ready()) { ZCHECK(false); return; }

  const int ACCOUNTS = 6;
  const int64_t OPENING = 1000;

  session(store.memory);
  for (int32_t i = 1; i <= ACCOUNTS; i++) {
    Account row(i, "acct", OPENING);
    store.memory->online_insert(row);
  }
  store.memory->commit_transaction();

  session(store.memory);
  const int64_t expected = total_balance(store.memory);
  ZCHECK_EQ(expected, OPENING * ACCOUNTS);
  store.memory->commit_transaction();

  const int TRANSFERS = 5;
  std::atomic<int> completed(0);
  std::atomic<int> skipped(0);

  fan_out(ACCOUNTS, [&] (int t) {
      const int32_t from_id = (int32_t)(t + 1);
      const int32_t to_id = (int32_t)(((t + 1) % ACCOUNTS) + 1);

      for (int n = 0; n < TRANSFERS; n++) {
	try {
	  // SERIALIZABLE: two transfers touching the same account must not run
	  // at the same time, otherwise both read the same balance and one of
	  // the two updates is lost.
	  session(store.memory, IsolationLevel::SERIALIZABLE);

	  Account from, to;
	  if (!find_by_id(store.memory, from_id, from) || !find_by_id(store.memory, to_id, to)) {
	    store.memory->rollback_transaction();
	    skipped++;
	    continue;
	  }

	  const int64_t amount = 10;
	  Account from_new(from_id, "acct", from.balance.value() - amount);
	  Account to_new(to_id, "acct", to.balance.value() + amount);

	  store.memory->online_update(from, from_new);
	  store.memory->online_update(to, to_new);
	  store.memory->commit_transaction();
	  completed++;
	} catch (...) {
	  try { store.memory->rollback_transaction(); } catch (...) { }
	  skipped++;
	}
      }
    });

  session(store.memory);
  ZCHECK_EQ(count_rows(store.memory), (size_t)ACCOUNTS);
  ZCHECK_EQ(total_balance(store.memory), expected);
  ZCHECK(completed.load() + skipped.load() == ACCOUNTS * TRANSFERS);
}

ZTEST(Concurrency, readers_never_observe_a_torn_row)
{
  Store store("conc-readers");
  if (!store.ready()) { ZCHECK(false); return; }

  const int ROWS = 20;
  session(store.memory);
  for (int32_t i = 1; i <= ROWS; i++) {
    // owner length is derived from the balance, so a torn row is detectable.
    Account row(i, std::string((size_t)i, 'a'), i);
    store.memory->online_insert(row);
  }
  store.memory->commit_transaction();

  std::atomic<bool> writing(true);
  std::atomic<int> torn(0);
  std::atomic<int> reads(0);

  std::thread writer([&] () {
      session(store.memory);
      for (int round = 0; round < 30; round++) {
	for (int32_t i = 1; i <= ROWS; i++) {
	  Account current;
	  if (!find_by_id(store.memory, i, current)) continue;
	  Account updated(i, std::string((size_t)i, 'a'), i);
	  store.memory->online_update(current, updated);
	}
	store.memory->commit_transaction();
	session(store.memory);
      }
      writing = false;
      store.memory->commit_transaction();
    });

  std::vector<std::thread> readers;
  for (int r = 0; r < 3; r++) {
    readers.push_back(std::thread([&] () {
	  session(store.memory);
	  while (writing.load()) {
	    std::vector<Account> rows = select_all(store.memory);
	    for (size_t i = 0; i < rows.size(); i++) {
	      if (rows[i].id.is_null().value() || rows[i].balance.is_null().value()) { torn++; continue; }
	      // The invariant every row was written with.
	      if ((int64_t)rows[i].owner.to_std_string().size() != rows[i].balance.value()) torn++;
	      if (rows[i].id.value() != (int32_t)rows[i].balance.value()) torn++;
	    }
	    reads++;
	  }
	  store.memory->commit_transaction();
	}));
  }

  writer.join();
  for (size_t i = 0; i < readers.size(); i++) readers[i].join();

  ZCHECK(reads.load() > 0);
  ZCHECK_EQ(torn.load(), 0);
}

// One writer stages a row, several readers scan at once. None of them may see
// it, and once the writer rolls back it must be gone for good.
ZTEST(Concurrency, concurrent_readers_never_see_staged_work)
{
  Store store("conc-visibility");
  if (!store.ready()) { ZCHECK(false); return; }

  const int READERS = 4;
  std::atomic<int> staged(0);
  std::atomic<int> leaked(0);
  std::atomic<int> finished(0);

  std::thread writer([&] () {
      session(store.memory);
      Account row(1, "staged", 1);
      store.memory->online_insert(row);
      staged = 1;

      // Hold it open long enough for every reader to reach the row.
      std::this_thread::sleep_for(std::chrono::milliseconds(400));
      store.memory->rollback_transaction();
    });

  std::vector<std::thread> readers;
  for (int r = 0; r < READERS; r++) {
    readers.push_back(std::thread([&] () {
	  for (int waited = 0; waited < 5000 && staged.load() == 0; waited += 5)
	    std::this_thread::sleep_for(std::chrono::milliseconds(5));

	  session(store.memory);
	  try {
	    leaked += (int)count_rows(store.memory);
	  } catch (...) {
	    // A lock wait timeout is an acceptable outcome; a dirty read is not.
	  }
	  store.memory->rollback_transaction();
	  finished++;
	}));
  }

  writer.join();
  for (size_t i = 0; i < readers.size(); i++) readers[i].join();

  ZCHECK_EQ(finished.load(), READERS);
  ZCHECK_EQ(leaked.load(), 0);

  session(store.memory);
  ZCHECK_EQ(count_rows(store.memory), (size_t)0);
}

// Sessions that each hold a lock the others want form a genuine cycle. The
// engine has no deadlock detector, so the lock wait timeout is what has to stop
// the whole store wedging.
ZTEST(Concurrency, a_lock_cycle_times_out_instead_of_wedging)
{
  Store store("conc-deadlock");
  if (!store.ready()) { ZCHECK(false); return; }

  const int original_timeout = Memory::lock_wait_timeout_ms;
  Memory::lock_wait_timeout_ms = 500;

  const int SESSIONS = 4;
  std::atomic<int> staged(0);
  std::atomic<int> finished(0);
  std::atomic<int> timed_out(0);
  std::atomic<int> dirty(0);

  fan_out(SESSIONS, [&] (int t) {
      session(store.memory);

      Account mine((int32_t)(t + 1), "staged", t + 1);
      store.memory->online_insert(mine);
      staged++;

      for (int waited = 0; waited < 5000 && staged.load() < SESSIONS; waited += 5)
	std::this_thread::sleep_for(std::chrono::milliseconds(5));

      // Everyone now scans while everyone holds a lock: the cycle.
      try {
	std::vector<Account> rows = select_all(store.memory);
	for (size_t i = 0; i < rows.size(); i++)
	  if (rows[i].id.value() != (int32_t)(t + 1)) dirty++;
      } catch (...) {
	timed_out++;
      }

      store.memory->rollback_transaction();
      finished++;
    });

  Memory::lock_wait_timeout_ms = original_timeout;

  // The point is that all four threads came back at all.
  ZCHECK_EQ(finished.load(), SESSIONS);
  ZCHECK_EQ(dirty.load(), 0);
  ZCHECK(timed_out.load() > 0);

  // And nothing uncommitted survived the pile-up.
  session(store.memory);
  ZCHECK_EQ(count_rows(store.memory), (size_t)0);
}

ZTEST(Concurrency, mixed_workload_leaves_a_consistent_store)
{
  Store store("conc-mixed");
  if (!store.ready()) { ZCHECK(false); return; }

  const int SEED = 30;
  session(store.memory);
  for (int32_t i = 1; i <= SEED; i++) {
    Account row(i, "seed", i);
    store.memory->online_insert(row);
  }
  store.memory->commit_transaction();

  std::atomic<int> errors(0);
  std::atomic<int> retries(0);

  fan_out(4, [&] (int t) {
      try {
	session(store.memory);

	if (t % 2 == 0) {
	  // Updater: rewrite a slice, keeping id == balance.
	  for (int32_t i = 1 + t; i <= SEED; i += 4) {
	    Account current;
	    if (!find_by_id(store.memory, i, current)) continue;
	    Account updated(i, "updated", i);
	    store.memory->online_update(current, updated);
	  }
	} else {
	  // Inserter: add rows that keep the same invariant.
	  for (int32_t i = 0; i < 10; i++) {
	    const int32_t id = SEED + t * 100 + i + 1;
	    Account row(id, "added", id);
	    store.memory->online_insert(row);
	  }
	}

	store.memory->commit_transaction();
      } catch (const ZiguratException&) {
	// A lock wait timeout means this transaction lost the race and rolled
	// back, which is a legitimate outcome. Corruption is not.
	retries++;
	try { store.memory->rollback_transaction(); } catch (...) { }
      } catch (...) {
	errors++;
	try { store.memory->rollback_transaction(); } catch (...) { }
      }
    });

  ZCHECK_EQ(errors.load(), 0);

  session(store.memory);
  std::vector<Account> rows = select_all(store.memory);

  // Whatever interleaving happened, every surviving row must still satisfy
  // id == balance, and no id may appear twice.
  int broken = 0;
  std::vector<int32_t> ids;
  for (size_t i = 0; i < rows.size(); i++) {
    if (rows[i].id.is_null().value() || rows[i].balance.is_null().value()) { broken++; continue; }
    if (rows[i].id.value() != (int32_t)rows[i].balance.value()) broken++;
    ids.push_back(rows[i].id.value());
  }
  std::sort(ids.begin(), ids.end());

  ZCHECK_EQ(broken, 0);
  ZCHECK(std::adjacent_find(ids.begin(), ids.end()) == ids.end());
  // Two inserter threads add ten rows each, unless one of them backed off.
  ZCHECK(rows.size() >= (size_t)SEED);
  ZCHECK(rows.size() <= (size_t)(SEED + 20));
}
