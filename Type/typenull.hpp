
#ifndef __NULL_HPP__
#define __NULL_HPP__

#include "typeobject.hpp"

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

#endif // __NULL_HPP__
