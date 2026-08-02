#include "option.h"


namespace Zigurat
{

  Option::Option(std::string nattr, std::string nkey, std::string nvalue)
  {
    this->attr = nattr;
    this->key = nkey;
    this->value = nvalue;
  }

  Option::~Option()
  {

  }

}

