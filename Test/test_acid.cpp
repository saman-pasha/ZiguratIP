#include "ztest.h"
#include "dbfixture.h"

using namespace Zigurat;

// The table's identity. The hash key just has to be distinct from the reserved
// ones and from BTreeRecord's.
std::string Account::name = "Zigurat::Test::Account";
hashkey_t Account::hash_key = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa,
			       0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01, 0x02, 0x03, 0x04, 0x05};


// ---------------------------------------------------------------------------
// The fixture itself, before anything is concluded from it.
// ---------------------------------------------------------------------------

ZTEST(Atomicity, fixture_inserts_and_reads_back)
{
  Store store("acid-smoke");
  if (!store.ready()) { ZCHECK(false); return; }

  Account row(1, "alice", 100);
  ZCHECK_NOTHROW(store.memory->online_insert(row));
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account found;
  ZCHECK(find_by_id(store.memory, 1, found));
  ZCHECK_EQ(found.id.value(), 1);
  ZCHECK_STR(found.owner.to_std_string(), "alice");
  ZCHECK_EQ(found.balance.value(), (int64_t)100);
}


// ---------------------------------------------------------------------------
// Atomicity: all of a transaction, or none of it.
// ---------------------------------------------------------------------------

ZTEST(Atomicity, committed_insert_is_visible)
{
  Store store("atom-commit");
  if (!store.ready()) { ZCHECK(false); return; }

  Account row(10, "committed", 500);
  store.memory->online_insert(row);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account found;
  ZCHECK(find_by_id(store.memory, 10, found));
  ZCHECK_EQ(found.balance.value(), (int64_t)500);
}

ZTEST(Atomicity, rolled_back_insert_leaves_nothing_behind)
{
  Store store("atom-rollback");
  if (!store.ready()) { ZCHECK(false); return; }

  Account row(20, "discarded", 500);
  store.memory->online_insert(row);
  store.memory->rollback_transaction();
  store.memory->begin_transaction();

  Account found;
  ZCHECK(!find_by_id(store.memory, 20, found));
  ZCHECK_EQ(count_rows(store.memory), (size_t)0);
}

ZTEST(Atomicity, a_rolled_back_batch_is_discarded_whole)
{
  Store store("atom-batch");
  if (!store.ready()) { ZCHECK(false); return; }

  // One committed row to prove the rollback is selective.
  Account keeper(1, "keeper", 1);
  store.memory->online_insert(keeper);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  for (int32_t i = 100; i < 110; i++) {
    Account row(i, "batch", i);
    store.memory->online_insert(row);
  }
  ZCHECK_EQ(count_rows(store.memory), (size_t)11);   // visible to its own writer

  store.memory->rollback_transaction();
  store.memory->begin_transaction();

  ZCHECK_EQ(count_rows(store.memory), (size_t)1);
}

ZTEST(Atomicity, rolled_back_update_restores_the_previous_row)
{
  Store store("atom-update");
  if (!store.ready()) { ZCHECK(false); return; }

  Account row(30, "before", 100);
  store.memory->online_insert(row);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account current;
  ZCHECK(find_by_id(store.memory, 30, current));

  Account updated(30, "after", 999);
  store.memory->online_update(current, updated);
  store.memory->rollback_transaction();
  store.memory->begin_transaction();

  Account found;
  ZCHECK(find_by_id(store.memory, 30, found));
  ZCHECK_STR(found.owner.to_std_string(), "before");
  ZCHECK_EQ(found.balance.value(), (int64_t)100);
  ZCHECK_EQ(count_rows(store.memory), (size_t)1);
}

ZTEST(Atomicity, committed_update_replaces_the_row)
{
  Store store("atom-update-commit");
  if (!store.ready()) { ZCHECK(false); return; }

  Account row(31, "before", 100);
  store.memory->online_insert(row);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account current;
  ZCHECK(find_by_id(store.memory, 31, current));

  Account updated(31, "after", 999);
  store.memory->online_update(current, updated);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account found;
  ZCHECK(find_by_id(store.memory, 31, found));
  ZCHECK_STR(found.owner.to_std_string(), "after");
  ZCHECK_EQ(found.balance.value(), (int64_t)999);
  // An update is one row, not two.
  ZCHECK_EQ(count_rows(store.memory), (size_t)1);
}

ZTEST(Atomicity, rolled_back_delete_keeps_the_row)
{
  Store store("atom-delete");
  if (!store.ready()) { ZCHECK(false); return; }

  Account row(40, "survivor", 7);
  store.memory->online_insert(row);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account current;
  ZCHECK(find_by_id(store.memory, 40, current));

  store.memory->online_delete(current);
  store.memory->rollback_transaction();
  store.memory->begin_transaction();

  Account found;
  ZCHECK(find_by_id(store.memory, 40, found));
  ZCHECK_EQ(found.balance.value(), (int64_t)7);
}

ZTEST(Atomicity, committed_delete_removes_the_row)
{
  Store store("atom-delete-commit");
  if (!store.ready()) { ZCHECK(false); return; }

  Account row(41, "doomed", 7);
  store.memory->online_insert(row);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account current;
  ZCHECK(find_by_id(store.memory, 41, current));

  store.memory->online_delete(current);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account found;
  ZCHECK(!find_by_id(store.memory, 41, found));
  ZCHECK_EQ(count_rows(store.memory), (size_t)0);
}


// ---------------------------------------------------------------------------
// Consistency: invariants that must hold across whatever the engine does.
// ---------------------------------------------------------------------------

ZTEST(Consistency, a_committed_transfer_preserves_the_total)
{
  Store store("cons-transfer");
  if (!store.ready()) { ZCHECK(false); return; }

  Account a(1, "a", 300);
  Account b(2, "b", 700);
  store.memory->online_insert(a);
  store.memory->online_insert(b);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  const int64_t before = total_balance(store.memory);
  ZCHECK_EQ(before, (int64_t)1000);

  Account from, to;
  ZCHECK(find_by_id(store.memory, 1, from));
  ZCHECK(find_by_id(store.memory, 2, to));

  Account from_new(1, "a", from.balance.value() - 250);
  Account to_new(2, "b", to.balance.value() + 250);
  store.memory->online_update(from, from_new);
  store.memory->online_update(to, to_new);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  ZCHECK_EQ(total_balance(store.memory), before);
  ZCHECK_EQ(count_rows(store.memory), (size_t)2);

  Account check;
  ZCHECK(find_by_id(store.memory, 1, check));
  ZCHECK_EQ(check.balance.value(), (int64_t)50);
}

ZTEST(Consistency, a_rolled_back_transfer_preserves_the_total)
{
  Store store("cons-rollback");
  if (!store.ready()) { ZCHECK(false); return; }

  Account a(1, "a", 300);
  Account b(2, "b", 700);
  store.memory->online_insert(a);
  store.memory->online_insert(b);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account from, to;
  find_by_id(store.memory, 1, from);
  find_by_id(store.memory, 2, to);

  // A half-finished transfer: take from one side and abandon it.
  Account from_new(1, "a", from.balance.value() - 250);
  store.memory->online_update(from, from_new);
  store.memory->rollback_transaction();
  store.memory->begin_transaction();

  ZCHECK_EQ(total_balance(store.memory), (int64_t)1000);
  ZCHECK_EQ(count_rows(store.memory), (size_t)2);
}

ZTEST(Consistency, row_values_survive_a_full_write_read_cycle)
{
  Store store("cons-values");
  if (!store.ready()) { ZCHECK(false); return; }

  // Deliberately awkward: a long owner and extreme balances.
  Account big(1, std::string(200, 'x'), 9223372036854775807LL);
  Account negative(2, "neg", -9223372036854775807LL);
  Account zero(3, "", 0);

  store.memory->online_insert(big);
  store.memory->online_insert(negative);
  store.memory->online_insert(zero);
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  Account found;
  ZCHECK(find_by_id(store.memory, 1, found));
  ZCHECK_EQ(found.owner.to_std_string().size(), (size_t)200);
  ZCHECK_EQ(found.balance.value(), (int64_t)9223372036854775807LL);

  ZCHECK(find_by_id(store.memory, 2, found));
  ZCHECK_EQ(found.balance.value(), (int64_t)-9223372036854775807LL);

  ZCHECK(find_by_id(store.memory, 3, found));
  ZCHECK_EQ(found.balance.value(), (int64_t)0);
}

ZTEST(Consistency, many_rows_are_all_accounted_for)
{
  Store store("cons-bulk");
  if (!store.ready()) { ZCHECK(false); return; }

  const int32_t COUNT = 200;
  int64_t expected_total = 0;

  for (int32_t i = 1; i <= COUNT; i++) {
    Account row(i, "bulk", i);
    store.memory->online_insert(row);
    expected_total += i;
  }
  store.memory->commit_transaction();
  store.memory->begin_transaction();

  ZCHECK_EQ(count_rows(store.memory), (size_t)COUNT);
  ZCHECK_EQ(total_balance(store.memory), expected_total);

  // Every id must appear exactly once.
  std::vector<Account> rows = select_all(store.memory);
  std::vector<int> seen(COUNT + 1, 0);
  for (size_t i = 0; i < rows.size(); i++) {
    const int32_t id = rows[i].id.value();
    if (id >= 1 && id <= COUNT) seen[id]++;
  }
  int duplicates = 0, missing = 0;
  for (int32_t i = 1; i <= COUNT; i++) {
    if (seen[i] == 0) missing++;
    if (seen[i] > 1) duplicates++;
  }
  ZCHECK_EQ(missing, 0);
  ZCHECK_EQ(duplicates, 0);
}


// ---------------------------------------------------------------------------
// Durability: a commit has to outlive the process that made it.
// ---------------------------------------------------------------------------

ZTEST(Durability, committed_rows_survive_a_reopen)
{
  const std::string tag = "durable";

  {
    Store store(tag);
    if (!store.ready()) { ZCHECK(false); return; }

    Account a(1, "persisted", 111);
    Account b(2, "also", 222);
    store.memory->online_insert(a);
    store.memory->online_insert(b);
    store.memory->commit_transaction();

    store.detach();      // closes the files, leaving them on disk
    store.hexmap_path.clear();   // stop the destructor erasing them
    store.data_path.clear();
  }

  {
    Store reopened(tag, false);   // reopen without truncating
    if (!reopened.ready()) { ZCHECK(false); return; }

    Account found;
    ZCHECK(find_by_id(reopened.memory, 1, found));
    ZCHECK_STR(found.owner.to_std_string(), "persisted");
    ZCHECK_EQ(found.balance.value(), (int64_t)111);

    ZCHECK(find_by_id(reopened.memory, 2, found));
    ZCHECK_EQ(found.balance.value(), (int64_t)222);

    ZCHECK_EQ(count_rows(reopened.memory), (size_t)2);
  }
}

ZTEST(Durability, rolled_back_rows_do_not_survive_a_reopen)
{
  const std::string tag = "durable-rollback";

  {
    Store store(tag);
    if (!store.ready()) { ZCHECK(false); return; }

    Account keeper(1, "keeper", 1);
    store.memory->online_insert(keeper);
    store.memory->commit_transaction();
    store.memory->begin_transaction();

    Account ghost(2, "ghost", 2);
    store.memory->online_insert(ghost);
    store.memory->rollback_transaction();

    store.detach();
    store.hexmap_path.clear();
    store.data_path.clear();
  }

  {
    Store reopened(tag, false);
    if (!reopened.ready()) { ZCHECK(false); return; }

    Account found;
    ZCHECK(find_by_id(reopened.memory, 1, found));
    ZCHECK(!find_by_id(reopened.memory, 2, found));
    ZCHECK_EQ(count_rows(reopened.memory), (size_t)1);
  }
}
