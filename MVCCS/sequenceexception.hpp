#include <string>
#include "zexception.hpp"


#ifndef __SEQUENCEEXCEPTION_HPP__
#define __SEQUENCEEXCEPTION_HPP__

namespace Zigurat
{

  class SequenceException : public ZiguratException
  {
  public:
    SequenceException() = delete;
    SequenceException(std::string msg) : ZiguratException(5330, msg) { }
  };

}

#endif // __SEQUENCEEXCEPTION_HPP__
