
#ifndef __BTREEVALUE_H__
#define __BTREEVALUE_H__

#include "basetable.h"

namespace Zigurat
{

  class BTreeValue : public BaseTable
  {
  public:
    Long value;

    int64_t pack_size() override
    {
      return binarystream::pack_size(this->value);
    }

    void prepare() override { }
    void map() override { }
    void unmap() override { }
  
    friend binarystream& operator<<(binarystream& outstream, const BTreeValue& value)
    {
      outstream.pack(value.value);
      return outstream;
    }
  
    friend binarystream& operator>>(binarystream& instream, BTreeValue& value)
    {
      instream.unpack(value.value);
      return instream;
    }
  };
  
}

#endif // __BTREEVALUE_H__
