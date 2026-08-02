
#ifndef __BASETABLE_H__
#define __BASETABLE_H__

#include "pointer.h"
#include <string>

namespace Zigurat
{

  class BaseTable
  {
  public:
    static std::string name;
    static hashkey_t hash_key;
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

#endif // __BASETABLE_H__

