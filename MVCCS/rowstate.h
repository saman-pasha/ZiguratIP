
#ifndef __ROWSTATE_H__
#define __ROWSTATE_H__

namespace Zigurat {

  enum class RowState : uint8_t
  {
    NONE = 0,
    INSERTED = 4,
    UPDATED = 8,
    DELETED = 12
  };

}

#endif // __ROWSTATE_H__
