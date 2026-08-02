#include "typenull.hpp"
#include "typestring.hpp"


namespace Zigurat
{

  Null::Null()
  {
    this->_pointer = nullptr;
  }
  
  Null::operator bool()
  {
    return *this->_pointer;
  }

  String Null::TO_STRING()
  {
    return String();
  }

}
