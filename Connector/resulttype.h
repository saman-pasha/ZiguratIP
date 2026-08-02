
#ifndef __RESULTTYPE_H__
#define __RESULTTYPE_H__

#include <cstdint>

namespace Zigurat
{

  enum class ResultType : uint8_t
  {
    SUCCESSFUL_DONE,
    CURSOR_OPEN,
    CURSOR_FETCH,
    CURSOR_CLOSE,
    RETURN_VALUE,
    EXCEPTION_THROWN
  };

}

#endif // __RESULTTYPE_H__
