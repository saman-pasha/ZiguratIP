#include <string>
#include "zexception.h"


#ifndef __SEQUENCEEXCEPTION_H__
#define __SEQUENCEEXCEPTION_H__

namespace Zigurat
{

  class SequenceException : public ZiguratException
  {
  public:
    SequenceException() = delete;
    SequenceException(std::string msg) : ZiguratException(5330, msg) { }
  };

}

#endif // __SEQUENCEEXCEPTION_H__
