#include "ztest.h"
#include "dbfixture.h"
#include "globals.h"
#include "memory.h"
#include "btreerecord.h"
#include "btreeindex.h"
#include "isolationlevel.h"
#include "filestream.h"
#include "bufferstream.h"
#include <cstdio>
#include <string>

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
