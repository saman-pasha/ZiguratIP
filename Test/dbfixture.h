
#ifndef __DBFIXTURE_H__
#define __DBFIXTURE_H__

// A concrete table plus a throwaway page store, so the ACID, isolation and
// concurrency suites can drive real rows through MVCCS.

#include "memory.h"
#include "basetable.h"
#include "globals.h"
#include "types.h"
#include "filestream.h"
#include "binarystream.h"
#include <string>
#include <vector>
#include <cstdio>

namespace Zigurat
{

  class Account : public BaseTable
  {
  public:
    using BaseTable::BaseTable;

    static std::string name;
    static hashkey_t hash_key;

    Int    id;
    String owner;
    Long   balance;

    Account() = default;

    Account(int32_t id, const std::string& owner, int64_t balance)
      : id(id), owner(owner), balance(balance) { }

    void prepare() override { }
    void map() override { }
    void unmap() override { }

    int64_t pack_size() override
    {
      return binarystream::pack_size(this->id, this->owner, this->balance);
    }

    friend binarystream& operator<<(binarystream& out, const Account& row)
    {
      out.pack(row.id, row.owner, row.balance);
      return out;
    }

    friend binarystream& operator>>(binarystream& in, Account& row)
    {
      in.unpack(row.id, row.owner, row.balance);
      return in;
    }
  };


  // A page store on a pair of temporary files, wired up the way load_memory does.
  class Store
  {
  public:
    std::string hexmap_path;
    std::string data_path;
    filestream  hexmap;
    filestream  data;
    Memory*     memory;

    explicit Store(const std::string& tag, bool truncate = true)
      : hexmap_path("/tmp/ziguratip-test-" + tag + "-hexmap"),
	data_path("/tmp/ziguratip-test-" + tag + "-data"),
	memory(nullptr)
    {
      if (truncate) {
	std::remove(hexmap_path.c_str());
	std::remove(data_path.c_str());
      }

      std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out | std::ios_base::binary;
      if (truncate) mode |= std::ios_base::trunc;

      hexmap.open(hexmap_path, mode);
      data.open(data_path, mode);

      if (hexmap.good() && data.good()) {
	memory = new Memory(hexmap, data, 8192);
	Globals::set_memory_hexmap_stream(&hexmap);
	Globals::set_memory_data_stream(&data);
	Globals::set_memory(memory);
      }
    }

    bool ready() const { return this->memory != nullptr; }

    // Detach without deleting the files, so a second Store can reopen them and
    // check that a committed row really survived.
    void detach()
    {
      // Clear the global first: ~Transaction consults it during teardown.
      Globals::set_memory(nullptr);
      Globals::set_memory_hexmap_stream(nullptr);
      Globals::set_memory_data_stream(nullptr);
      delete this->memory;
      this->memory = nullptr;
      this->hexmap.flush();
      this->data.flush();
      this->hexmap.close();
      this->data.close();
    }

    void erase()
    {
      if (!this->hexmap_path.empty()) std::remove(this->hexmap_path.c_str());
      if (!this->data_path.empty()) std::remove(this->data_path.c_str());
    }

    ~Store()
    {
      if (this->memory != nullptr) this->detach();
      this->erase();
    }
  };


  // --- helpers the suites share -------------------------------------------

  inline std::vector<Account> select_all(Memory* memory)
  {
    std::vector<Account> rows;
    memory->cursor<Account>([&rows] (Account& row) -> bool {
	rows.push_back(row);
	return true;   // keep going
      });
    return rows;
  }

  inline size_t count_rows(Memory* memory)
  {
    return select_all(memory).size();
  }

  inline int64_t total_balance(Memory* memory)
  {
    int64_t total = 0;
    std::vector<Account> rows = select_all(memory);
    for (size_t i = 0; i < rows.size(); i++)
      if (!rows[i].balance.is_null().value()) total += rows[i].balance.value();
    return total;
  }

  inline bool find_by_id(Memory* memory, int32_t id, Account& found)
  {
    bool hit = false;
    memory->cursor<Account>([&] (Account& row) -> bool {
	if (!row.id.is_null().value() && row.id.value() == id) {
	  found = row;
	  hit = true;
	  return false;   // stop at the first match
	}
	return true;
      });
    return hit;
  }

}

#endif // __DBFIXTURE_H__
