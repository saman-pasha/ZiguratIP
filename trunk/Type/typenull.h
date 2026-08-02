
#ifndef __NULL_H__
#define __NULL_H__

#include "typeobject.h"

namespace Zigurat
{

  class String;

  class Null : public Object<std::nullptr_t, TDByte::NULL_>
  {
  public:
    using Object<std::nullptr_t, TDByte::NULL_>::Object;
    Null();
    operator bool();
    virtual String TO_STRING();
  };
  
}

#endif // __NULL_H__
