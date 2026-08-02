// The B-tree index under bulk load.
//
// Every other suite drives MVCCS through a handful of rows, which is enough to
// exercise a single leaf node and nothing else. A BTreeIndex over Long keys has
// a branching factor of 65, so a tree only splits its root at 129 keys and only
// grows a third level in the tens of thousands. This suite loads enough rows to
// reach those levels and then checks each cursor against the answer computed in
// plain C++ over the same data.
//
// ZIGURAT_BTREE_ROWS overrides the row count, so the same cases can be run as a
// soak test:
//
//     ZIGURAT_BTREE_ROWS=200000 home/bin/Test BTree

#include "ztest.hpp"
#include "dbfixture.hpp"
#include "globals.hpp"
#include "memory.hpp"
#include "btreerecord.hpp"
#include "btreeindex.hpp"
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

using namespace Zigurat;


// --- the indexed table ------------------------------------------------------

// Modelled on what the Parsi compiler generates for
//
//     TABLE bench::item
//     BEGIN
//         COLUMN id       AS Long PRIMARY KEY;
//         COLUMN category AS String;
//         COLUMN score    AS Long;
//         INDEX KEY (category);
//         INDEX KEY (category, score);
//     END
//
// except that the indexes are pointers the fixture owns, so each case gets its
// own store rather than sharing process-wide statics.
class Item : public BaseTable
{
public:
  using BaseTable::BaseTable;

  static std::string name;
  static hashkey_t hash_key;

  Long   id;
  String category;
  Long   score;
  Short  bucket;

  typedef Long   Item::*id_t;
  typedef String Item::*category_t;
  typedef Long   Item::*score_t;
  typedef Short  Item::*bucket_t;

  static id_t       ID;
  static category_t CATEGORY;
  static score_t    SCORE;
  static bucket_t   BUCKET;

  static BTreeIndex<Item, Long>*           IDX_ID;              // unique
  static BTreeIndex<Item, String>*         IDX_CATEGORY;        // non-unique
  static BTreeIndex<Item, String, Long>*   IDX_CATEGORY_SCORE;  // composite
  static BTreeIndex<Item, Short>*          IDX_BUCKET;          // narrow keys

  Item() = default;

  Item(int64_t id, const std::string& category, int64_t score, int16_t bucket)
    : id(id), category(category), score(score), bucket(bucket) { }

  void prepare() override { }

  void map() override
  {
    if (Item::IDX_ID) Item::IDX_ID->map(*this);
    if (Item::IDX_CATEGORY) Item::IDX_CATEGORY->map(*this);
    if (Item::IDX_CATEGORY_SCORE) Item::IDX_CATEGORY_SCORE->map(*this);
    if (Item::IDX_BUCKET) Item::IDX_BUCKET->map(*this);
  }

  void unmap() override
  {
    if (Item::IDX_ID) Item::IDX_ID->unmap(*this);
    if (Item::IDX_CATEGORY) Item::IDX_CATEGORY->unmap(*this);
    if (Item::IDX_CATEGORY_SCORE) Item::IDX_CATEGORY_SCORE->unmap(*this);
    if (Item::IDX_BUCKET) Item::IDX_BUCKET->unmap(*this);
  }

  int64_t pack_size() override
  {
    return binarystream::pack_size(this->id, this->category, this->score, this->bucket);
  }

  friend binarystream& operator<<(binarystream& out, const Item& row)
  {
    out.pack(row.id, row.category, row.score, row.bucket);
    return out;
  }

  friend binarystream& operator>>(binarystream& in, Item& row)
  {
    in.unpack(row.id, row.category, row.score, row.bucket);
    return in;
  }
};

std::string Item::name = "Zigurat::Test::Item";
hashkey_t Item::hash_key = {0x7b, 0x71, 0xee, 0x02, 0x43, 0x9c, 0x18, 0x5a, 0xd0, 0x64,
			    0x2e, 0xc9, 0x35, 0x8f, 0x11, 0xa7, 0x60, 0xbb, 0x4d, 0x39};

Item::id_t       Item::ID       = &Item::id;
Item::category_t Item::CATEGORY = &Item::category;
Item::score_t    Item::SCORE    = &Item::score;
Item::bucket_t   Item::BUCKET   = &Item::bucket;

BTreeIndex<Item, Long>*         Item::IDX_ID             = nullptr;
BTreeIndex<Item, String>*       Item::IDX_CATEGORY       = nullptr;
BTreeIndex<Item, String, Long>* Item::IDX_CATEGORY_SCORE = nullptr;
BTreeIndex<Item, Short>*        Item::IDX_BUCKET         = nullptr;


// --- fixture ----------------------------------------------------------------

namespace
{

  // How many rows the bulk cases load. A Long index holds 128 keys per node, so
  // the default is deep enough to have split the root and filled a second level.
  int64_t bulk_rows()
  {
    const char* env = std::getenv("ZIGURAT_BTREE_ROWS");
    if (env == nullptr) return 2000;
    const long long n = std::atoll(env);
    return (n > 0) ? (int64_t)n : 2000;
  }

  const char* CATEGORIES[] = {"alpha", "beta", "gamma", "delta"};
  const int CATEGORY_COUNT = 4;

  // A node holds (sizeof(key) * 8) * 2 keys, so a Long index fits 128 per node
  // and needs about 16,500 rows before it grows a third level. A Short index
  // fits 32, and is three levels deep by a couple of thousand -- which is what
  // makes internal node splits affordable to cover on every run. The modulus
  // keeps the key inside a Short while leaving every id distinct at the sizes
  // the suite uses by default.
  const int16_t BUCKET_MODULUS = 30011;   // prime, just under 2^15

  // Deterministic but not sorted: consecutive ids must not arrive in key order,
  // or the tree would only ever be appended to on one side.
  int64_t scrambled(int64_t i, int64_t n)
  {
    return ((i * 7919) % n) + 1;
  }

  // A store with the catalogue bootstrapped and the three Item indexes
  // registered, mirroring what load_memory does at server startup.
  class Fixture
  {
  public:
    Store store;
    BTreeIndex<BTreeRecord, String>* catalogue;

    explicit Fixture(const std::string& tag, bool truncate = true)
      : store(tag, truncate), catalogue(nullptr)
    {
      if (!this->store.ready()) return;

      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = nullptr;
      this->catalogue = new BTreeIndex<BTreeRecord, String>
	(this->store.memory, "IDX_ZIGURAT_BTREERECORD_HASH_NAME", true, BTreeRecord::hash_name);
      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = this->catalogue;

      Item::IDX_ID = new BTreeIndex<Item, Long>
	(this->store.memory, "IDX_BENCH_ITEM_ID", true, Item::ID);
      Item::IDX_CATEGORY = new BTreeIndex<Item, String>
	(this->store.memory, "IDX_BENCH_ITEM_CATEGORY", false, Item::CATEGORY);
      Item::IDX_CATEGORY_SCORE = new BTreeIndex<Item, String, Long>
	(this->store.memory, "IDX_BENCH_ITEM_CATEGORY_SCORE", false, Item::CATEGORY, Item::SCORE);
      Item::IDX_BUCKET = new BTreeIndex<Item, Short>
	(this->store.memory, "IDX_BENCH_ITEM_BUCKET", false, Item::BUCKET);

      this->store.memory->commit_transaction();
    }

    bool ready() const { return this->store.ready() && this->catalogue != nullptr; }

    Memory* memory() { return this->store.memory; }

    // Load n rows and commit, leaving a fresh transaction open for the reads
    // that follow. Returns the rows as inserted, for the reference answers the
    // assertions are built from.
    std::vector<Item> load(int64_t n)
    {
      std::vector<Item> rows;
      rows.reserve((size_t)n);

      this->store.memory->begin_transaction();
      for (int64_t i = 0; i < n; i++) {
	const int64_t id = scrambled(i, n);
	Item row(id, CATEGORIES[i % CATEGORY_COUNT], (i % 97) * 10, (int16_t)(id % BUCKET_MODULUS));
	this->store.memory->online_insert(row);
	rows.push_back(row);
      }
      this->store.memory->commit_transaction();
      this->store.memory->begin_transaction();

      return rows;
    }

    ~Fixture()
    {
      delete Item::IDX_BUCKET;          Item::IDX_BUCKET = nullptr;
      delete Item::IDX_CATEGORY_SCORE;  Item::IDX_CATEGORY_SCORE = nullptr;
      delete Item::IDX_CATEGORY;        Item::IDX_CATEGORY = nullptr;
      delete Item::IDX_ID;              Item::IDX_ID = nullptr;
      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = nullptr;
      delete this->catalogue;           this->catalogue = nullptr;
    }
  };


  // --- what the index actually returned ------------------------------------

  struct Visit
  {
    std::vector<int64_t> ids;    // in the order the cursor produced them
    std::vector<int64_t> keys;   // the indexed column, same order

    size_t size() const { return this->ids.size(); }

    bool is_ascending() const
    {
      for (size_t i = 1; i < this->keys.size(); i++)
	if (this->keys[i] < this->keys[i - 1]) return false;
      return true;
    }

    std::vector<int64_t> sorted_ids() const
    {
      std::vector<int64_t> copy = this->ids;
      std::sort(copy.begin(), copy.end());
      return copy;
    }
  };

  // Every cursor has the same shape, so the six operators can share one driver.
  typedef void (BTreeIndex<Item, Long>::*long_cursor_t)(const Long, std::function<bool (Item&)>);

  Visit run(BTreeIndex<Item, Long>* index, long_cursor_t cursor, int64_t key)
  {
    Visit seen;
    (index->*cursor)(Long(key), [&seen] (Item& row) -> bool {
	seen.ids.push_back(row.id.value());
	seen.keys.push_back(row.id.value());
	return true;
      });
    return seen;
  }

  // The same answer, computed without the index.
  std::vector<int64_t> expect(const std::vector<Item>& rows,
			      std::function<bool (int64_t)> predicate)
  {
    std::vector<int64_t> ids;
    for (size_t i = 0; i < rows.size(); i++)
      if (predicate(rows[i].id.value())) ids.push_back(rows[i].id.value());
    std::sort(ids.begin(), ids.end());
    return ids;
  }

}


// --- bulk load --------------------------------------------------------------

ZTEST(BTree, bulk_load_walks_back_every_row_in_key_order)
{
  Fixture fixture("btree-bulk");
  if (!fixture.ready()) { ZCHECK(false); return; }

  const int64_t n = bulk_rows();
  const std::vector<Item> rows = fixture.load(n);
  ZCHECK_EQ((int64_t)rows.size(), n);

  Visit seen;
  Item::IDX_ID->cursor([&seen] (Item& row) -> bool {
      seen.ids.push_back(row.id.value());
      seen.keys.push_back(row.id.value());
      return true;
    });

  // Nothing lost, nothing repeated, and the walk is in key order.
  ZCHECK_EQ((int64_t)seen.size(), n);
  ZCHECK(seen.is_ascending());

  std::vector<int64_t> ids = seen.sorted_ids();
  ZCHECK(std::unique(ids.begin(), ids.end()) == ids.end());
}

// The tree changes shape as it grows -- one leaf, a split root, then internal
// levels -- and each shape is a different path through the cursor code.
ZTEST(BTree, every_tree_depth_answers_the_same_way)
{
  const int64_t sizes[] = {1, 8, 64, 128, 129, 260, 700, 1500};
  const int size_count = (int)(sizeof(sizes) / sizeof(sizes[0]));

  for (int s = 0; s < size_count; s++) {
    const int64_t n = sizes[s];
    Fixture fixture("btree-depth-" + std::to_string(n));
    if (!fixture.ready()) { ZCHECK(false); return; }

    fixture.load(n);

    int64_t counted = 0;
    Item::IDX_ID->cursor([&counted] (Item&) -> bool { counted++; return true; });
    ZCHECK_EQ(counted, n);
  }
}


// A tree deep enough to have split an internal node, not just its root. The
// Short index gets there at the default row count; the same code on the Long
// index needs an order of magnitude more rows.
ZTEST(BTree, internal_node_splits_keep_every_key)
{
  Fixture fixture("btree-deep");
  if (!fixture.ready()) { ZCHECK(false); return; }

  const int64_t n = bulk_rows();
  const std::vector<Item> rows = fixture.load(n);

  std::vector<int64_t> keys;
  int64_t counted = 0;
  Item::IDX_BUCKET->cursor([&keys, &counted] (Item& row) -> bool {
      keys.push_back(row.bucket.value());
      counted++;
      return true;
    });

  ZCHECK_EQ(counted, n);
  ZCHECK(std::is_sorted(keys.begin(), keys.end()));

  // And the operators over that same deep tree.
  const int16_t probe = rows[rows.size() / 2].bucket.value();

  int64_t below = 0, at = 0, above = 0;
  Item::IDX_BUCKET->cursor_less_than(Short(probe), [&below] (Item&) -> bool { below++; return true; });
  Item::IDX_BUCKET->cursor_equal(Short(probe), [&at] (Item&) -> bool { at++; return true; });
  Item::IDX_BUCKET->cursor_greater_than(Short(probe), [&above] (Item&) -> bool { above++; return true; });

  int64_t expect_below = 0, expect_at = 0, expect_above = 0;
  for (size_t i = 0; i < rows.size(); i++) {
    const int16_t k = rows[i].bucket.value();
    if (k < probe) expect_below++;
    else if (k == probe) expect_at++;
    else expect_above++;
  }

  ZCHECK_EQ(below, expect_below);
  ZCHECK_EQ(at, expect_at);
  ZCHECK_EQ(above, expect_above);
  ZCHECK_EQ(below + at + above, n);
}


// --- the six operators ------------------------------------------------------

ZTEST(BTree, equality_operators_over_a_bulk_index)
{
  Fixture fixture("btree-eq");
  if (!fixture.ready()) { ZCHECK(false); return; }

  const int64_t n = bulk_rows();
  const std::vector<Item> rows = fixture.load(n);
  const int64_t probe = n / 3;

  Visit eq = run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_equal, probe);
  ZCHECK_EQ((int64_t)eq.size(), (int64_t)1);
  if (eq.size() == 1) ZCHECK_EQ(eq.ids[0], probe);

  // A key that was never inserted.
  Visit miss = run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_equal, n + 1000);
  ZCHECK_EQ((int64_t)miss.size(), (int64_t)0);

  Visit ne = run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_not_equal, probe);
  ZCHECK_EQ((int64_t)ne.size(), n - 1);
  ZCHECK(ne.sorted_ids() == expect(rows, [probe] (int64_t k) { return k != probe; }));
}

ZTEST(BTree, range_operators_over_a_bulk_index)
{
  Fixture fixture("btree-range");
  if (!fixture.ready()) { ZCHECK(false); return; }

  const int64_t n = bulk_rows();
  const std::vector<Item> rows = fixture.load(n);
  const int64_t probe = n / 3;

  Visit lt = run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_less_than, probe);
  ZCHECK_EQ((int64_t)lt.size(), probe - 1);
  ZCHECK(lt.sorted_ids() == expect(rows, [probe] (int64_t k) { return k < probe; }));

  Visit le = run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_less_than_equal, probe);
  ZCHECK_EQ((int64_t)le.size(), probe);
  ZCHECK(le.sorted_ids() == expect(rows, [probe] (int64_t k) { return k <= probe; }));

  Visit gt = run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_greater_than, probe);
  ZCHECK_EQ((int64_t)gt.size(), n - probe);
  ZCHECK(gt.sorted_ids() == expect(rows, [probe] (int64_t k) { return k > probe; }));

  Visit ge = run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_greater_than_equal, probe);
  ZCHECK_EQ((int64_t)ge.size(), n - probe + 1);
  ZCHECK(ge.sorted_ids() == expect(rows, [probe] (int64_t k) { return k >= probe; }));

  // The two halves of a range split have to add up to the whole tree.
  ZCHECK_EQ((int64_t)(lt.size() + ge.size()), n);
  ZCHECK_EQ((int64_t)(le.size() + gt.size()), n);
}

// The boundaries: below the smallest key, above the largest, and exactly on
// each end.
ZTEST(BTree, range_operators_at_the_edges_of_the_key_space)
{
  Fixture fixture("btree-edges");
  if (!fixture.ready()) { ZCHECK(false); return; }

  const int64_t n = bulk_rows();
  fixture.load(n);   // ids are a permutation of 1..n

  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_less_than, 1).size(), (int64_t)0);
  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_less_than_equal, 1).size(), (int64_t)1);
  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_greater_than, n).size(), (int64_t)0);
  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_greater_than_equal, n).size(), (int64_t)1);

  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_greater_than, 0).size(), n);
  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_less_than, n + 1).size(), n);
  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_less_than_equal, n + 1).size(), n);
  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_greater_than_equal, 0).size(), n);
}


// --- duplicate keys ---------------------------------------------------------

// A non-unique index buckets every row under one key. With four categories over
// n rows each bucket holds a quarter of the table.
ZTEST(BTree, a_non_unique_index_returns_every_row_in_a_bucket)
{
  Fixture fixture("btree-dups");
  if (!fixture.ready()) { ZCHECK(false); return; }

  const int64_t n = bulk_rows();
  fixture.load(n);

  int64_t total = 0;
  for (int c = 0; c < CATEGORY_COUNT; c++) {
    int64_t counted = 0;
    Item::IDX_CATEGORY->cursor_equal(String(CATEGORIES[c]), [&counted] (Item&) -> bool {
	counted++;
	return true;
      });

    // Row i takes CATEGORIES[i % 4], so the buckets differ by at most one.
    const int64_t expected = (n / CATEGORY_COUNT) + ((n % CATEGORY_COUNT > c) ? 1 : 0);
    ZCHECK_EQ(counted, expected);
    total += counted;
  }

  ZCHECK_EQ(total, n);
}

ZTEST(BTree, a_unique_index_refuses_a_duplicate_key)
{
  Fixture fixture("btree-unique");
  if (!fixture.ready()) { ZCHECK(false); return; }

  fixture.memory()->begin_transaction();
  Item first(42, "alpha", 1, 42);
  ZCHECK_NOTHROW(fixture.memory()->online_insert(first));
  fixture.memory()->commit_transaction();
  fixture.memory()->begin_transaction();

  Item clash(42, "beta", 2, 42);
  ZCHECK_THROWS(fixture.memory()->online_insert(clash));
  fixture.memory()->rollback_transaction();
  fixture.memory()->begin_transaction();

  // The row that was already there is untouched.
  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_equal, 42).size(),
	    (int64_t)1);
}


// --- string keys ------------------------------------------------------------

ZTEST(BTree, string_keys_order_and_range_the_same_way)
{
  Fixture fixture("btree-strings");
  if (!fixture.ready()) { ZCHECK(false); return; }

  const int64_t n = bulk_rows();
  fixture.load(n);

  std::vector<std::string> seen;
  Item::IDX_CATEGORY->cursor([&seen] (Item& row) -> bool {
      seen.push_back(row.category.to_std_string());
      return true;
    });

  ZCHECK_EQ((int64_t)seen.size(), n);
  ZCHECK(std::is_sorted(seen.begin(), seen.end()));

  // "beta" and "delta" sort below "gamma"; "alpha" below all of them.
  int64_t below_gamma = 0;
  Item::IDX_CATEGORY->cursor_less_than(String("gamma"), [&below_gamma] (Item&) -> bool {
      below_gamma++;
      return true;
    });

  int64_t gamma_and_above = 0;
  Item::IDX_CATEGORY->cursor_greater_than_equal(String("gamma"), [&gamma_and_above] (Item&) -> bool {
      gamma_and_above++;
      return true;
    });

  ZCHECK_EQ(below_gamma + gamma_and_above, n);
}


// --- composite index --------------------------------------------------------

// (category, score) is a two level index: the first level buckets by category
// and hands the second level out as a nested BTreeIndex.
ZTEST(BTree, a_composite_index_ranges_on_its_second_column)
{
  Fixture fixture("btree-composite");
  if (!fixture.ready()) { ZCHECK(false); return; }

  const int64_t n = bulk_rows();
  const std::vector<Item> rows = fixture.load(n);

  // Everything under one category, through both levels.
  int64_t counted = 0;
  Item::IDX_CATEGORY_SCORE->cursor_equal(String("alpha"),
					 [&counted] (BTreeIndex<Item, Long>& scores) -> bool {
      scores.cursor([&counted] (Item&) -> bool { counted++; return true; });
      return true;
    });

  int64_t expected_alpha = 0;
  for (size_t i = 0; i < rows.size(); i++)
    if (rows[i].category.to_std_string() == "alpha") expected_alpha++;
  ZCHECK_EQ(counted, expected_alpha);

  // A range on the second column, inside one bucket of the first.
  const int64_t cut = 400;
  int64_t high = 0;
  Item::IDX_CATEGORY_SCORE->cursor_equal(String("alpha"),
					 [&high, cut] (BTreeIndex<Item, Long>& scores) -> bool {
      scores.cursor_greater_than_equal(Long(cut), [&high] (Item&) -> bool { high++; return true; });
      return true;
    });

  int64_t expected_high = 0;
  for (size_t i = 0; i < rows.size(); i++)
    if (rows[i].category.to_std_string() == "alpha" && rows[i].score.value() >= cut) expected_high++;
  ZCHECK_EQ(high, expected_high);
}


// --- maintenance ------------------------------------------------------------

ZTEST(BTree, deleting_rows_takes_them_out_of_the_index)
{
  Fixture fixture("btree-delete");
  if (!fixture.ready()) { ZCHECK(false); return; }

  const int64_t n = bulk_rows();
  fixture.load(n);

  // Collect a slice through the index, then delete it.
  std::vector<Item> doomed;
  Item::IDX_ID->cursor_less_than_equal(Long(20), [&doomed] (Item& row) -> bool {
      doomed.push_back(row);
      return true;
    });
  ZCHECK_EQ((int64_t)doomed.size(), (int64_t)20);

  for (size_t i = 0; i < doomed.size(); i++)
    fixture.memory()->online_delete(doomed[i]);
  fixture.memory()->commit_transaction();
  fixture.memory()->begin_transaction();

  // The index and a plain table scan have to agree about what is left. An
  // unmapped row leaves its key in the tree with an empty bucket, and walking
  // over one of those must not end the cursor.
  int64_t scanned = 0;
  fixture.memory()->cursor<Item>([&scanned] (Item&) -> bool { scanned++; return true; });
  ZCHECK_EQ(scanned, n - (int64_t)doomed.size());

  int64_t remaining = 0;
  Item::IDX_ID->cursor([&remaining] (Item&) -> bool { remaining++; return true; });
  ZCHECK_EQ(remaining, n - (int64_t)doomed.size());

  // The deleted keys are gone, and the ones just above them are not.
  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_less_than_equal, 20).size(),
	    (int64_t)0);
  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_equal, 21).size(),
	    (int64_t)1);
}

// The index lives in the page store, so it has to come back after a restart
// without being rebuilt.
ZTEST(BTree, the_index_survives_a_reopen)
{
  const int64_t n = bulk_rows();
  const std::string tag = "btree-reopen";

  {
    Fixture fixture(tag);
    if (!fixture.ready()) { ZCHECK(false); return; }
    fixture.load(n);
    fixture.store.memory->commit_transaction();

    fixture.store.detach();            // closes the files, leaving them on disk
    fixture.store.hexmap_path.clear(); // stop the destructor erasing them
    fixture.store.data_path.clear();
  }

  // Reopening the same files: the catalogue finds the indexes by name and picks
  // up their root addresses rather than starting empty.
  Fixture reopened(tag, false);
  if (!reopened.ready()) { ZCHECK(false); return; }

  int64_t counted = 0;
  Item::IDX_ID->cursor([&counted] (Item&) -> bool { counted++; return true; });
  ZCHECK_EQ(counted, n);

  ZCHECK_EQ((int64_t)run(Item::IDX_ID, &BTreeIndex<Item, Long>::cursor_equal, n / 2).size(),
	    (int64_t)1);
}
