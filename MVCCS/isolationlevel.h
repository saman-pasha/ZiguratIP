
#ifndef __ISOLATIONLEVEL_H__
#define __ISOLATIONLEVEL_H__

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

#endif // __ISOLATIONLEVEL_H__
