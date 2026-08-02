
#ifndef __EXPRCOMPILER_H__
#define __EXPRCOMPILER_H__

#include "configuration.h"
#include <sstream>
#include <string>
#include <list>
#include <initializer_list>

namespace Zigurat
{

  class Compiler;
  class Expression;

  class ExprCompiler
  {
  private:
    const Compiler& _compiler;
    std::list< std::pair<std::string, std::string> > _locals;
    Configuration _catalog;
    void _object(const Expression&, std::stringstream&) const;
  public:
    ExprCompiler(const Compiler&, std::initializer_list<const Expression>);
    void compile(const Expression&, std::stringstream&) const;
  };

}

#endif // __EXPRCOMPILER_H__
