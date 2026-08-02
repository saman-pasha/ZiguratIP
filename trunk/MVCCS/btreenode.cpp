#include "btreenode.h"
#include "binarystream.h"


namespace Zigurat
{

  BTreeNode::BTreeNode() : 
    degree((int16_t)0), is_internal(false), keys_address((int64_t)-1) 
  { 
  
  }

  int64_t BTreeNode::pack_size()
  {
    return binarystream::pack_size(this->degree, this->is_internal, this->keys_address);
  }

  void BTreeNode::prepare()
  { 

  }
  
  void BTreeNode::map()
  { 

  }
  
  void BTreeNode::unmap() 
  { 

  }
  
  binarystream& operator<<(binarystream& outstream, const BTreeNode& node)
  {
    outstream.pack(node.degree, node.is_internal, node.keys_address);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, BTreeNode& node)
  {
    instream.unpack(node.degree, node.is_internal, node.keys_address);
    return instream;
  }    

}
