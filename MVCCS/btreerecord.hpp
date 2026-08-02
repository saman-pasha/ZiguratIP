
#ifndef __BTREERECORD_HPP__
#define __BTREERECORD_HPP__

#include "basetable.hpp"
#include "typebool.hpp"
#include "typeshort.hpp"
#include "typelong.hpp"
#include "typestring.hpp"

namespace Zigurat
{

  template <typename ..._Args>
  class BTreeIndex;

  class BTreeRecord : public BaseTable
  {
  private:
    String _hash_name;
    Bool _is_unique;
    Short _branching_factor;
    Long _root_address;

  public:
    using BaseTable::BaseTable;
    static std::string name;
    static hashkey_t hash_key;

    BTreeRecord() = default;

    typedef String BTreeRecord::*hash_name_t;
    typedef Bool BTreeRecord::*is_unique_t;
    typedef Short BTreeRecord::*branching_factor_t;
    typedef Long BTreeRecord::*root_address_t;

    static hash_name_t hash_name;
    static is_unique_t is_unique;
    static branching_factor_t branching_factor;
    static root_address_t root_address;

    static BTreeIndex<BTreeRecord, String>* IDX_ZIGURAT_BTREERECORD_HASH_NAME;

    void prepare() override;
    void map() override;
    void unmap() override;
    int64_t pack_size() override;
    friend binarystream& operator<<(binarystream&, const BTreeRecord&);
    friend binarystream& operator>>(binarystream&, BTreeRecord&);
  };
  
}

#endif // __BTREERECORD_HPP__
