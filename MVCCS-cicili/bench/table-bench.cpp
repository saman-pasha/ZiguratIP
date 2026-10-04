// The insert side of a real table, measured as the table grows: blocks of
// rows shaped like a downstream table of Bitcoin transactions -- an id
// from a counter (the server's PRIMARY KEY from a sequence), a composite
// (kb, height) index, an index on a hashed transaction id, and a body of
// hex -- one transaction per block, as a client committing a block at a
// time would. Each block's inserts and its commit are timed apart, and a
// line is printed at blocks 1, 2, 5, 10, 20, 50, ... so a per-block cost
// that grows with the table shows as itself.
//
//   table_bench BLOCKS [INDEXES] [ROWS]
//
// INDEXES is any of p (the id, unique), h (the composite (kb, height)), t
// (the hashed txid), default "pht"; ROWS is a block's transactions,
// default 2000. A block is 2,000 transactions: every 50th of 2,000 hex
// characters, the middle one of 80,000 cut into two rows, the rest of 500
// -- 2,001 rows. STORE_MAP=1 opens the store mapped, as the server does;
// PAGE sets the page size (default 65536). Built and run by build.sh
// at twenty blocks, or by hand with its compile line.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <chrono>
#include "engine.hpp"
#include "filestream.hpp"
#include "mapstream.hpp"

static Zigurat::binarystream* open_store (const char* path, bool fresh) {
  const std::ios_base::openmode mode = std::ios::in | std::ios::out | (fresh ? std::ios::trunc : (std::ios_base::openmode)0);
  const char* env = getenv("STORE_MAP");
  if (env && env[0] == '1') return new Zigurat::mapstream(std::string(path), mode);
  return new Zigurat::filestream(std::string(path), mode);
}

static uint8_t ROW_KEY[20] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20 };
static uint8_t PK_KEY[20]  = { 21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40 };
static uint8_t KH_KEY[20]  = { 41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60 };
static uint8_t KH_DEP[20]  = { 61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80 };
static uint8_t TX_KEY[20]  = { 81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100 };
static BTreeIndex PK, KH, TX;
static bool use_pk = false, use_kh = false, use_tx = false;

struct Row : public BaseTable {
  int64_t id = 0, kb = 0, height = 0, idx = 0, seq = 0, txid = 0;
  const std::string* body = nullptr;
  std::string owned;
  int64_t pack_size () override { return 6 * 8 + 4 + (int64_t)(body ? body->size() : owned.size()); }
  void pack (Zigurat::binarystream& io) override {
    io.write_std_long(id); io.write_std_long(kb); io.write_std_long(height);
    io.write_std_long(idx); io.write_std_long(seq); io.write_std_long(txid);
    const std::string& b = body ? *body : owned;
    io.write_std_int((int32_t)b.size()); io.write(b.data(), (std::streamsize)b.size());
  }
  void unpack (Zigurat::binarystream& io) override {
    io.read_std_long(id); io.read_std_long(kb); io.read_std_long(height);
    io.read_std_long(idx); io.read_std_long(seq); io.read_std_long(txid);
    int32_t n = 0; io.read_std_int(n); owned.resize((size_t)n); io.read(&owned[0], n); body = nullptr;
  }
  void map (void*) override {
    if (use_pk) bt_map(&PK, id, pointer.address);
    if (use_kh) { int64_t ks[2] = { kb, height }; bt_map_multi(&KH, ks, pointer.address); }
    if (use_tx) bt_map(&TX, txid, pointer.address);
  }
  void unmap (void*) override {}
};

static void attach (BTreeIndex* x, Memory* m, const char* name, uint8_t* key, int64_t cat,
                    int64_t unique, int64_t levels, uint8_t* dep) {
  x->m = m; x->name = name; x->hash_key = intern_key(key); x->table_key = ROW_KEY;
  x->catalogue_id = cat; x->is_unique = unique; x->branching_factor = 65; x->min_degree = 64;
  x->max_degree = 128; x->root_address = -1; x->record_pointer = pointer_null(); x->levels = levels;
  x->is_dependent = 0; x->dep_hash_key = dep ? intern_key(dep) : nullptr;
  bt_select_record(x);
}

static double now () {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool checkpoint (long b) {
  for (long m : { 1L, 2L, 5L }) { long x = b; while (x > m && x % 10 == 0) x /= 10; if (x == m) return true; }
  return false;
}

// a 64-bit hash spread over the row's (height, idx): what a String txid folds to
static int64_t txid_of (int64_t h, int64_t i) {
  uint64_t x = (uint64_t)h * 1000003ULL + (uint64_t)i;
  x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
  return (int64_t)(x & 0x7fffffffffffffffULL);
}

int main (int argc, char** argv) {
  const long blocks = argc > 1 ? atol(argv[1]) : 10;
  const char* which = argc > 2 ? argv[2] : "pht";
  const long T = argc > 3 ? atol(argv[3]) : 2000;
  const char* ps = getenv("PAGE");
  const int64_t page = ps ? atoll(ps) : 65536;
  use_pk = strchr(which, 'p') != nullptr; use_kh = strchr(which, 'h') != nullptr; use_tx = strchr(which, 't') != nullptr;
  setvbuf(stdout, nullptr, _IONBF, 0);

  const char* dir = getenv("TMPDIR"); std::string base = std::string(dir ? dir : "/tmp") + "/mvccs-table-bench";
  std::string hp = base + "-hexmap.bin", dp = base + "-data.bin";
  remove(hp.c_str()); remove(dp.c_str());
  Zigurat::binarystream* h = open_store(hp.c_str(), true);
  Zigurat::binarystream* d = open_store(dp.c_str(), true);
  Memory* m = engine_memory_new();
  memory_open(m, h, d, page);
  if (use_pk) attach(&PK, m, "PK", PK_KEY, 515151, 1, 1, nullptr);
  if (use_kh) attach(&KH, m, "KH", KH_KEY, 525252, 0, 2, KH_DEP);
  if (use_tx) attach(&TX, m, "TX", TX_KEY, 535353, 0, 1, nullptr);

  // the bodies: hex characters, the longest row a part of the page less 600
  const long part = page - 600;
  std::string b500(500, 'a'), b2000(2000, 'b'), bpart(part < 80000 ? part : 80000, 'c'),
              brest(part < 80000 ? 80000 - part : 0, 'd');
  for (size_t i = 0; i < b500.size(); i++) b500[i] = "0123456789abcdef"[(i * 7) & 15];

  printf("table_bench: %ld blocks of %ld transactions, indexes \"%s\", page %lld, %s\n",
         blocks, T, which, (long long)page, getenv("STORE_MAP") ? "mapped" : "filebuf");
  int64_t id = 0;
  double sum_ins = 0, sum_com = 0, max_blk = 0;
  for (long b = 1; b <= blocks; b++) {
    double t0 = now();
    begin_transaction(m);
    long rows = 0;
    for (long i = 0; i < T; i++) {
      const std::string* parts[2] = { &b500, nullptr };
      if (i == T / 2) { parts[0] = &bpart; parts[1] = brest.empty() ? nullptr : &brest; }
      else if (i % 50 == 49) parts[0] = &b2000;
      for (int s = 0; s < 2 && parts[s]; s++) {
        Row r; r.id = ++id; r.kb = 7; r.height = b; r.idx = i; r.seq = s; r.txid = txid_of(b, i); r.body = parts[s];
        online_insert(m, ROW_KEY, &r);
        rows++;
      }
    }
    double t1 = now();
    commit_transaction(m);
    double t2 = now();
    sum_ins += t1 - t0; sum_com += t2 - t1; if (t2 - t0 > max_blk) max_blk = t2 - t0;
    if (checkpoint(b))
      printf("  block %5ld: %ld rows, inserts %.4f s (%.1f us/row), commit %.4f s, total %.4f s\n",
             b, rows, t1 - t0, (t1 - t0) / rows * 1e6, t2 - t1, t2 - t0);
  }
  printf("  average: inserts %.4f s, commit %.4f s a block; slowest block %.4f s\n",
         sum_ins / blocks, sum_com / blocks, max_blk);
  engine_memory_delete(m);
  return 0;
}
