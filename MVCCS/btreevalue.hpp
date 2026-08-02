
#ifndef __BTREEVALUE_HPP__
#define __BTREEVALUE_HPP__

#include "basetable.hpp"

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

#endif // __BTREEVALUE_HPP__
