
#ifndef __BTREENODE_HPP__
#define __BTREENODE_HPP__

#include "basetable.hpp"
#include "typebool.hpp"
#include "typeshort.hpp"
#include "typelong.hpp"

namespace Zigurat
{

  class BTreeNode : public BaseTable
  {
  public:
    Short degree;
    Bool is_internal;
    Long keys_address;
    BTreeNode();
    void prepare() override;
    void map() override;
    void unmap() override;
    int64_t pack_size() override;
    friend binarystream& operator<<(binarystream&, const BTreeNode&);  
    friend binarystream& operator>>(binarystream&, BTreeNode&);
  };
  
}

#endif // __BTREENODE_HPP__
