
#ifndef __POINTER_H__
#define __POINTER_H__

#include "memorybase.h"

namespace Zigurat
{

  class Pointer
  {
  public:
    hashkey_ptr hash_key;
    int64_t address;
    int64_t size;
    Pointer();
    Pointer(hashkey_ptr, int64_t, int64_t);
    Pointer(const Pointer&);
    Pointer(Pointer&&);
    Pointer& operator=(const Pointer&);
    Pointer& operator=(Pointer&&);
    virtual ~Pointer();
  };

}

#endif // __POINTER_H__
