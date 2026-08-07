
#ifndef __BTREEVALUE_HPP__
#define __BTREEVALUE_HPP__


#include <cstdint>
#include "basetable.hpp"

namespace Zigurat
{

  // One row address, under the key that indexes it.
  //
  // These used to be found by hash key: every key got a bucket of its own, named
  // after the address of the key node, and a lookup asked Memory::_cursor for
  // everything in that bucket. A bucket is a page file at minimum, so an index
  // over n rows cost n pages -- 8 KB of store for eight bytes of address -- and
  // the Memory Viewer showed hundreds of page files each holding one record.
  //
  // So the values of a key are a chain instead. Each one names the next, the key
  // names the first, and they all allocate under the index's own hash key, which
  // is where its nodes and keys already lived. The index packs like a table
  // because it is stored like one.
  //
  // Nothing is ever unlinked. A deleted value keeps its place in the chain with
  // its control block marked, exactly as a deleted row keeps its chunks, and the
  // walk decides what is visible -- which is what makes a rolled back delete
  // reappear on its own.
  class BTreeValue : public BaseTable
  {
  public:
    Long value;

    // The next value of the same key, or -1. Set once, when the value is linked
    // at the head of its key's chain.
    Long next_address;

    BTreeValue() : next_address((int64_t)-1) { }

    int64_t pack_size() override
    {
      return binarystream::pack_size(this->value, this->next_address);
    }

    void prepare() override { }
    void map() override { }
    void unmap() override { }

    friend binarystream& operator<<(binarystream& outstream, const BTreeValue& value)
    {
      outstream.pack(value.value, value.next_address);
      return outstream;
    }

    friend binarystream& operator>>(binarystream& instream, BTreeValue& value)
    {
      instream.unpack(value.value, value.next_address);
      return instream;
    }
  };

}

#endif // __BTREEVALUE_HPP__
