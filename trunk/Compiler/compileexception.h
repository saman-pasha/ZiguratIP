
#ifndef __COMPILEEXCEPTION_H__
#define __COMPILEEXCEPTION_H__

#include "zexception.h"
#include "expression.h"
#include <string>
#include <sstream>

namespace Zigurat
{

  class CompileException : public ZiguratException
  {
  public:
    CompileException() = delete;
    CompileException(std::string, const Expression&);
  };

}

#endif // __COMPILEEXCEPTION_H__

