
#ifndef __ROWSTATE_HPP__
#define __ROWSTATE_HPP__


#include <cstdint>
namespace Zigurat {

  enum class RowState : uint8_t
  {
    NONE = 0,
    INSERTED = 4,
    UPDATED = 8,
    DELETED = 12
  };

}

#endif // __ROWSTATE_HPP__
