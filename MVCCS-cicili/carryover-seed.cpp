// The golden store, made. carryover-new.cpp opens a store under golden/ and
// proves rows carry and indexes rebuild over them; carryover-old.cpp is the
// program that first wrote those bytes and CANNOT BE BUILT ANY MORE, because
// it needs MVCCS/ and MVCCS/ is retired. The bytes it left were meant to be
// checked in beside it -- and never were: .gitignore excludes *.bin, so the
// acceptance SKIPped on every machine that had no private copy, which by now
// is every machine. This writes them again.
//
// WHAT THESE BYTES ARE, said plainly: a store written by the CICILI engine at
// the commit that adds this file, in the shape carryover-old.cpp wrote --
// five rows under the same hash key, one of them rewritten so a version chain
// exists. They are not the retired C++ engine's bytes; nobody has those. What
// they are is FROZEN: from here on the acceptance opens a store no current
// engine wrote, and the first change that stops it opening is the one the
// case exists to catch. That is the whole of what a golden store is for.
//
// The format has not moved since the C++ engine -- which is why a store
// written now is still the right input, and why carryover-new.cpp needed no
// change to read it.
//
//   sh build.sh          # builds the engine this links against, then:
//   c++ -O3 -std=gnu++17 carryover-seed.cpp -o carryover_seed \
//       -I. -I../home/include -L../home/lib -lMVCCS -lCore -lStreamIO -lType \
//       -lpthread -Wl,-rpath,../home/lib
//   ./carryover_seed     # rewrites golden/carryover-{hexmap,data}.bin
//
// Run it only to make the golden store again on purpose. Rerunning it and
// committing the result throws away everything the old bytes were pinning.

#include <cstdio>
#include <string>
#include <sys/stat.h>
#include "engine.hpp"
#include "typeint.hpp"
#include "typelong.hpp"
#include "typestring.hpp"
#include "filestream.hpp"

// the same 20 bytes carryover-old.cpp stamped its rows with, which is what
// carryover-new.cpp looks for
static uint8_t CARRY_KEY[20] = {0xca,0x11,0x40,0x0e,0x12,0x34,0x56,0x78,0x9a,0xbc,
                                0xde,0xf0,0x0f,0xed,0xcb,0xa9,0x87,0x65,0x43,0x21};

// No map/unmap: the golden store carries the TABLE and no index of its own.
// carryover-new.cpp builds its own index and rebuilds it from these rows,
// which is the half of the acceptance that would mean nothing if the store
// arrived with one.
struct CarryRow : public BaseTable {
  Zigurat::Int    id;
  Zigurat::String owner;
  Zigurat::Long   balance;
  int64_t pack_size () override {
    return Zigurat::binarystream::pack_size(this->id, this->owner, this->balance);
  }
  void pack (Zigurat::binarystream& io) override { io.pack(this->id, this->owner, this->balance); }
  void unpack (Zigurat::binarystream& io) override { io.unpack(this->id, this->owner, this->balance); }
  void map (void*) override { }
  void unmap (void*) override { }
};

static Memory* MEM = nullptr;

struct Find { long want; CarryRow row; bool found; };
static bool find_cb (void* u, Pointer* p) {
  Find* f = (Find*)u;
  CarryRow it; it.pointer = *p;
  read_row(MEM, &it);
  if (it.id.value() == f->want) { f->row = it; f->found = true; return false; }
  return true;
}

int main ()
{
  ::mkdir("golden", 0755);
  ::remove("golden/carryover-hexmap.bin");
  ::remove("golden/carryover-data.bin");

  Zigurat::filestream h(std::string("golden/carryover-hexmap.bin"),
                        std::ios::in | std::ios::out | std::ios::trunc);
  Zigurat::filestream d(std::string("golden/carryover-data.bin"),
                        std::ios::in | std::ios::out | std::ios::trunc);
  if (!h.good() || !d.good()) { printf("FAIL cannot create the golden store\n"); return 1; }

  MEM = engine_memory_new();
  memory_open(MEM, (Zigurat::binarystream*)&h, (Zigurat::binarystream*)&d, 8192);
  globals_set_memory(MEM);

  // five rows, as carryover-old.cpp wrote them
  begin_transaction(MEM);
  for (int i = 1; i <= 5; i++) {
    CarryRow row;
    row.id = Zigurat::Int(i);
    row.owner = Zigurat::String(std::string("acct-") + std::to_string(i));
    row.balance = Zigurat::Long((int64_t)(i * 100));
    online_insert(MEM, CARRY_KEY, &row);
  }
  commit_transaction(MEM);

  // and the rewrite, so the store carries a superseded version as well as a
  // committed one -- the reader checks it reads 999 and not 300
  begin_transaction(MEM);
  {
    Find f{3, {}, false};
    engine_cursor(MEM, CARRY_KEY, &f, find_cb);
    if (!f.found) { printf("FAIL row 3 is not there to rewrite\n"); return 1; }
    CarryRow next;
    next.id = Zigurat::Int(3);
    next.owner = Zigurat::String(std::string("acct-3"));
    next.balance = Zigurat::Long((int64_t)999);
    online_update(MEM, CARRY_KEY, &f.row, &next);
  }
  commit_transaction(MEM);

  struct stat sh, sd;
  ::stat("golden/carryover-hexmap.bin", &sh);
  ::stat("golden/carryover-data.bin", &sd);
  printf("golden store written: hexmap %lld bytes, data %lld bytes\n",
         (long long)sh.st_size, (long long)sd.st_size);
  return 0;
}
