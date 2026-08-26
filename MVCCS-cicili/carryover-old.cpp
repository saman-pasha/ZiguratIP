// The OLD engine's half of the carry-over acceptance: write rows the way
// the C++ MVCCS always wrote them -- five inserts, one update so a
// version chain exists -- into a store the NEW engine will open next.
//
// HISTORICAL: this file needed MVCCS/ to build, and MVCCS/ is retired.
// The last store it wrote is checked in under golden/ and stays the
// acceptance's input; the source stays as the record of how those bytes
// were made. carryover-new.cpp is the half that still runs.

#include <cstdio>
#include <string>
#include "../MVCCS/globals.hpp"
#include "../MVCCS/basetable.hpp"
#include "../MVCCS/memory.hpp"
#include "../StreamIO/filestream.hpp"

using namespace Zigurat;

class CarryRow : public BaseTable
{
public:
  using BaseTable::BaseTable;

  static std::string name;
  static hashkey_t hash_key;

  Int    id;
  String owner;
  Long   balance;

  CarryRow() = default;
  CarryRow(int32_t id, const std::string& owner, int64_t balance)
    : id(id), owner(owner), balance(balance) { }

  void prepare() override { }
  void map() override { }
  void unmap() override { }

  int64_t pack_size() override
  {
    return binarystream::pack_size(this->id, this->owner, this->balance);
  }

  friend binarystream& operator<<(binarystream& out, const CarryRow& row)
  {
    out.pack(row.id, row.owner, row.balance);
    return out;
  }

  friend binarystream& operator>>(binarystream& in, CarryRow& row)
  {
    in.unpack(row.id, row.owner, row.balance);
    return in;
  }
};

std::string CarryRow::name = "Zigurat::Test::CarryRow";
hashkey_t CarryRow::hash_key = {0xca,0x11,0x40,0x0e,0x12,0x34,0x56,0x78,0x9a,0xbc,
                                0xde,0xf0,0x0f,0xed,0xcb,0xa9,0x87,0x65,0x43,0x21};

int main ()
{
  remove("/tmp/mvccs-carryover-hexmap.bin");
  remove("/tmp/mvccs-carryover-data.bin");
  filestream h(std::string("/tmp/mvccs-carryover-hexmap.bin"),
               std::ios::in | std::ios::out | std::ios::trunc);
  filestream d(std::string("/tmp/mvccs-carryover-data.bin"),
               std::ios::in | std::ios::out | std::ios::trunc);

  Memory m(h, d, 8192);

  m.begin_transaction();
  for (int32_t i = 1; i <= 5; i++) {
    CarryRow row(i, "acct-" + std::to_string(i), (int64_t)i * 100);
    m.online_insert(row);
  }
  m.commit_transaction();

  // one row rewritten, so the store carries a version chain -- the new
  // engine has to pick the committed version, not the superseded one
  m.begin_transaction();
  CarryRow current;
  bool hit = false;
  m.cursor<CarryRow>([&] (CarryRow& row) -> bool {
      if (row.id.value() == 3) { current = row; hit = true; return false; }
      return true;
    });
  if (!hit) { printf("FAIL the old engine lost its own row 3\n"); return 1; }
  CarryRow next(3, "acct-3", 999);
  m.online_update(current, next);
  m.commit_transaction();

  printf("ok   the old engine wrote five rows and one rewrite\n");
  return 0;
}
