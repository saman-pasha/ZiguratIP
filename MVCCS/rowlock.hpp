
#ifndef __ROWLOCK_HPP__
#define __ROWLOCK_HPP__


#include <cstdint>
namespace Zigurat {

  enum class RowLock : uint8_t
  {
    NONE = 0,
    SHARED = 1,
    EXCLUSIVE = 2
  };

  inline RowLock operator|(RowLock l, RowLock r)
  {
    return static_cast<RowLock>(static_cast<uint8_t>(l) | static_cast<uint8_t>(r));
  }

  inline RowLock operator&(RowLock l, RowLock r)
  {
    return static_cast<RowLock>(static_cast<uint8_t>(l) & static_cast<uint8_t>(r));
  }

}

#endif // __ROWLOCK_HPP__
