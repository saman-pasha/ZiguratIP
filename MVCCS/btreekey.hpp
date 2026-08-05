
#ifndef __BTREEKEY_HPP__
#define __BTREEKEY_HPP__


#include <cstdint>
#include "basetable.hpp"

namespace Zigurat
{

  template <typename K>
  class BTreeKey : public BaseTable
  {
  public:    
    Long left_node_address;
    Long left_address;
    Long right_address;
    Long right_node_address;
    
    Long dependents_address;
    K key;

    BTreeKey() : left_node_address(-1), left_address(-1), right_address(-1), right_node_address(-1), dependents_address(-1) { }
    
    int64_t pack_size() override
    {
      return binarystream::pack_size(this->left_node_address, this->left_address,
				     this->right_address, this->right_node_address,
				     this->dependents_address, this->key);
    }

    void prepare() override { }
    void map() override { }
    void unmap() override { }
  
    friend binarystream& operator<<(binarystream& outstream, const BTreeKey<K>& key)
    {
      outstream.pack(key.left_node_address, key.left_address,
		     key.right_address, key.right_node_address,
		     key.dependents_address, key.key);
      return outstream;
    }
  
    friend binarystream& operator>>(binarystream& instream, BTreeKey<K>& key)
    {
      instream.unpack(key.left_node_address, key.left_address,
		      key.right_address, key.right_node_address,
		      key.dependents_address, key.key);
      return instream;
    }
  };
  
}

#endif // __BTREEKEY_HPP__
