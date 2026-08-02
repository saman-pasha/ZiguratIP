
#ifndef __ISOLATIONLEVEL_HPP__
#define __ISOLATIONLEVEL_HPP__

#include <cstdint>

namespace Zigurat
{

  enum class IsolationLevel : uint8_t
  {
    READ_UNCOMMITTED,
    READ_COMMITTED,
    REPEATABLE_READ,
    SNAPSHOT,
    SERIALIZABLE
  };

}

#endif // __ISOLATIONLEVEL_HPP__
