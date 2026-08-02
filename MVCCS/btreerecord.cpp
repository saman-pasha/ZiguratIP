#include "btreerecord.hpp"
#include "binarystream.hpp"
#include "btreeindex.hpp"
#include <tuple>


namespace Zigurat
{

  std::string BTreeRecord::name = "Zigurat::BTreeRecord";
  hashkey_t BTreeRecord::hash_key = {0xaf, 0x43, 0xa1, 0xbf, 0x4a, 0x83, 0xcb, 0x66, 0xdb, 0x15, 
				     0xd7, 0xfb, 0x8d, 0x64, 0x8d, 0x9d, 0x87, 0x4e, 0x71, 0x5e};
    
  BTreeRecord::hash_name_t BTreeRecord::hash_name = &BTreeRecord::_hash_name;
  BTreeRecord::is_unique_t BTreeRecord::is_unique = &BTreeRecord::_is_unique;
  BTreeRecord::branching_factor_t BTreeRecord::branching_factor = &BTreeRecord::_branching_factor;
  BTreeRecord::root_address_t BTreeRecord::root_address = &BTreeRecord::_root_address;

  BTreeIndex<BTreeRecord, String>* BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME = nullptr;

  void BTreeRecord::prepare() 
  { 

  }
  
  void BTreeRecord::map() 
  { 
    if (BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME)
      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME->map(*this);
  }
  
  void BTreeRecord::unmap()
  { 
    if (BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME)
      BTreeRecord::IDX_ZIGURAT_BTREERECORD_HASH_NAME->unmap(*this);
  }
  
  int64_t BTreeRecord::pack_size()
  {
    return binarystream::pack_size(this->_hash_name, this->_is_unique, this->_branching_factor, this->_root_address);
  }

  binarystream& operator<<(binarystream& outstream, const BTreeRecord& record)
  {
    outstream.pack(record._hash_name, record._is_unique, record._branching_factor, record._root_address);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, BTreeRecord& record)
  {
    instream.unpack(record._hash_name, record._is_unique, record._branching_factor, record._root_address);
    return instream;
  }    

}
