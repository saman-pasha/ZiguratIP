
#ifndef __COMPILEEXCEPTION_HPP__
#define __COMPILEEXCEPTION_HPP__

#include "zexception.hpp"
#include "expression.hpp"
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

#endif // __COMPILEEXCEPTION_HPP__

