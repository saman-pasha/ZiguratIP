// The NEW engine's half of the carry-over acceptance: open the store
// carryover-old.cpp wrote with the C++ MVCCS and prove the claim the
// replacement rests on -- ROWS CARRY BYTE-IDENTICALLY, INDEXES REBUILD.
//
//   - the five rows read back through the Cicili engine's cursor, the
//     rewrite's committed version and not its superseded one;
//   - a fresh index built over the carried rows answers by key;
//   - the new engine UPDATES a carried row and INSERTS beside them,
//     and a rescan tells the whole story.
//
// The catalogue is NOT shared: the old engine's BTreeRecord rows are
// just settled rows of an internal table here, and a new-engine index
// makes its own record and REBUILDS from the table -- which is exactly
// what "indexes rebuild" means, and why nothing here reads the old
// catalogue at all.

#include <cstdio>
#include <cstring>
#include <string>
#include "engine.hpp"
#include "typeint.hpp"
#include "typelong.hpp"
#include "typestring.hpp"
#include "filestream.hpp"

// the same 20 bytes carryover-old.cpp stamped its rows with
static uint8_t CARRY_KEY[20] = {0xca,0x11,0x40,0x0e,0x12,0x34,0x56,0x78,0x9a,0xbc,
                                0xde,0xf0,0x0f,0xed,0xcb,0xa9,0x87,0x65,0x43,0x21};

// a fresh index over the carried rows, keyed on id
static uint8_t IDX_KEY[20] = {0xca,0x11,0x1d,0x00,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
static BTreeIndex IDX_CARRY_ID;

struct CarryRow : public BaseTable {
  Zigurat::Int    id;
  Zigurat::String owner;
  Zigurat::Long   balance;
  int64_t pack_size () override {
    return Zigurat::binarystream::pack_size(this->id, this->owner, this->balance);
  }
  void pack (Zigurat::binarystream& io) override { io.pack(this->id, this->owner, this->balance); }
  void unpack (Zigurat::binarystream& io) override { io.unpack(this->id, this->owner, this->balance); }
  void map (void*) override {
    bt_map(&IDX_CARRY_ID, (int64_t)this->id.value(), this->pointer.address);
  }
  void unmap (void*) override {
    bt_unmap(&IDX_CARRY_ID, (int64_t)this->id.value(), this->pointer.address);
  }
};

static Memory* MEM = nullptr;

static int failures = 0;
static void check (const char* what, long got, long want) {
  if (got == want) printf("ok   %-52s %ld\n", what, got);
  else { printf("FAIL %-52s got %ld want %ld\n", what, got, want); failures++; }
}
static void check_str (const char* what, const std::string& got, const std::string& want) {
  if (got == want) printf("ok   %-52s %s\n", what, got.c_str());
  else { printf("FAIL %-52s got [%s] want [%s]\n", what, got.c_str(), want.c_str()); failures++; }
}

struct Scan { long rows; long sum; long want_id; CarryRow row; bool found; bool remap; };
static bool scan_cb (void* u, Pointer* p) {
  Scan* s = (Scan*)u;
  CarryRow it; it.pointer = *p;
  read_row(MEM, &it);
  s->rows++; s->sum += it.balance.value();
  if (s->want_id != 0 && it.id.value() == s->want_id) { s->row = it; s->found = true; }
  if (s->remap) bt_map(&IDX_CARRY_ID, (int64_t)it.id.value(), p->address);
  return true;
}

int main ()
{
  Zigurat::filestream h(std::string("/tmp/mvccs-carryover-hexmap.bin"),
                        std::ios::in | std::ios::out);
  Zigurat::filestream d(std::string("/tmp/mvccs-carryover-data.bin"),
                        std::ios::in | std::ios::out);
  if (!h.good() || !d.good()) { printf("FAIL no carried store to open\n"); return 1; }

  MEM = engine_memory_new();
  memory_open(MEM, (Zigurat::binarystream*)&h, (Zigurat::binarystream*)&d, 8192);
  globals_set_memory(MEM);
  begin_transaction(MEM);

  // ---- the rows carry ---------------------------------------------------
  Scan s{0,0,3,{},false,false};
  engine_cursor(MEM, CARRY_KEY, &s, scan_cb);
  check("the old engine's five rows read back", s.rows, 5);
  // 100+200+999+400+500: the rewrite's committed version, not the 300
  check("with the rewrite's committed version", s.sum, 2199);
  check("row 3 carries the new balance", s.found ? (long)s.row.balance.value() : -1, 999);
  check_str("and its owner byte for byte",
            s.found ? s.row.owner.to_std_string() : std::string("gone"), "acct-3");

  // ---- a fresh index rebuilds over them ----------------------------------
  IDX_CARRY_ID.m = MEM;
  IDX_CARRY_ID.name = "IDX_CARRY_ID";
  IDX_CARRY_ID.hash_key = intern_key(IDX_KEY);
  IDX_CARRY_ID.table_key = CARRY_KEY;
  IDX_CARRY_ID.catalogue_id = 0x51de11;
  IDX_CARRY_ID.is_unique = 1;
  IDX_CARRY_ID.branching_factor = 65;
  IDX_CARRY_ID.min_degree = 64;
  IDX_CARRY_ID.max_degree = 128;
  IDX_CARRY_ID.root_address = -1;
  IDX_CARRY_ID.record_pointer = pointer_null();
  IDX_CARRY_ID.levels = 1;
  IDX_CARRY_ID.is_dependent = 0;
  IDX_CARRY_ID.dep_hash_key = nullptr;
  bt_select_record(&IDX_CARRY_ID);

  Scan rebuild{0,0,0,{},false,true};
  engine_cursor(MEM, CARRY_KEY, &rebuild, scan_cb);
  commit_transaction(MEM);
  begin_transaction(MEM);

  struct Hit { long hits; long balance; } hit{0,0};
  int64_t want_key[1] = {4};
  bt_cursor_equal_multi(&IDX_CARRY_ID, want_key, &hit,
    [] (void* u, Pointer* p) -> bool {
      Hit* hh = (Hit*)u;
      CarryRow it; it.pointer = *p;
      read_row(MEM, &it);
      hh->hits++; hh->balance = it.balance.value();
      return true;
    });
  check("a rebuilt index answers by key", hit.hits, 1);
  check("with the carried row behind it", hit.balance, 400);

  // ---- the new engine writes beside the carried rows ---------------------
  {
    Scan f{0,0,5,{},false,false};
    engine_cursor(MEM, CARRY_KEY, &f, scan_cb);
    CarryRow next;
    next.id = Zigurat::Int(5);
    next.owner = Zigurat::String(std::string("acct-5"));
    next.balance = Zigurat::Long((int64_t)501);
    online_update(MEM, CARRY_KEY, &f.row, &next);
  }
  {
    CarryRow born;
    born.id = Zigurat::Int(6);
    born.owner = Zigurat::String(std::string("acct-6"));
    born.balance = Zigurat::Long((int64_t)600);
    online_insert(MEM, CARRY_KEY, &born);
  }
  commit_transaction(MEM);
  begin_transaction(MEM);

  Scan after{0,0,5,{},false,false};
  engine_cursor(MEM, CARRY_KEY, &after, scan_cb);
  check("an update and an insert later, six rows", after.rows, 6);
  // 100+200+999+400+501+600
  check("and every version is the right one", after.sum, 2800);
  commit_transaction(MEM);

  printf("\ncarry-over old->new: %s (%d failures)\n",
         failures == 0 ? "all green" : "RED", failures);
  return failures != 0;
}
