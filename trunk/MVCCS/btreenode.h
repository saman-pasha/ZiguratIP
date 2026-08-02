
#ifndef __BTREENODE_H__
#define __BTREENODE_H__

#include "basetable.h"
#include "typebool.h"
#include "typeshort.h"
#include "typelong.h"

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

#endif // __BTREENODE_H__
