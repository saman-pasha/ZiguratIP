
#ifndef __BASETABLE_HPP__
#define __BASETABLE_HPP__


#include <cstdint>
#include "pointer.hpp"
#include <string>
#include <vector>

namespace Zigurat
{

  class BaseTable
  {
  public:
    static std::string name;

    // The same name split into the levels a permission is written in: the
    // schema, then the object. Every generated table and sequence shadows this
    // with its own, exactly as it does with name.
    static std::vector<std::string> path;

    static hashkey_t hash_key;

    // Reclaim the space of every settled dead entry in this table's indexes.
    //
    // Shadowed by every generated table with one that names its indexes, the
    // same way name, path and hash_key are shadowed. A table with no indexes,
    // and every internal record that is not a generated table, keeps this one
    // and truncating it reclaims its rows and nothing else.
    static void truncate_indexes();

    Pointer pointer;
    BaseTable();
    BaseTable(const Pointer&);
    BaseTable(Pointer&&);
    BaseTable(const BaseTable&);
    BaseTable(BaseTable&&);
    BaseTable& operator=(const BaseTable&);
    BaseTable& operator=(BaseTable&&);
    virtual void prepare() = 0;
    virtual void map() = 0;
    virtual void unmap() = 0;
    virtual int64_t pack_size() = 0;
    virtual ~BaseTable();
  };
    
}

#endif // __BASETABLE_HPP__

