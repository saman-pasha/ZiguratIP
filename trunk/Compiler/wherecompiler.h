
#ifndef __WHERECOMPILER_H__
#define __WHERECOMPILER_H__

#include "configuration.h"
#include "exprcompiler.h"
#include <sstream>
#include <string>
#include <list>
#include <functional>

namespace Zigurat
{

  class Compiler;
  class Expression;

  class WhereCompiler
  {
  private:
    const Compiler& _compiler;
    const Expression& _from;
    const std::string _tab;
    const std::string _name;
    const std::string _type_name;
    const Configuration _catalog;
    const ExprCompiler _expr;
    void _compile(const Expression&, std::stringstream&, int, std::function<void (int)>&, 
		  const Expression*) const;
    void _cursor(const Expression&, std::stringstream&, int, std::function <void (int)>&,
		 const Expression*, const Expression&, const std::string&,
		 const std::list<std::string>&, typename std::list<std::string>::iterator, 
		 const std::list<std::string>&, typename std::list<std::string>::iterator) const;
    std::string _cursor_name(const std::string&) const;

  public:
    WhereCompiler(const Compiler&, const Expression&);
    void compile(const Expression*, std::stringstream&, int, std::function<void (int)>) const;
  };

}

#endif // __WHERECOMPILER_H__
