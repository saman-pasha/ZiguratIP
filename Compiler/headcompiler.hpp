
#ifndef __HEADCOMPILER_HPP__
#define __HEADCOMPILER_HPP__

#include "configuration.hpp"
#include <sstream>
#include <string>
#include <list>
#include <initializer_list>

namespace Zigurat
{

  class Compiler;
  class Expression;

  class HeadCompiler
  {
  private:
    const Compiler& _compiler;
    std::list< std::pair<std::string, std::string> > _locals;
    Configuration _catalog;
    void _alias(const Expression&, std::stringstream&, int&) const;
  public:
    HeadCompiler(const Compiler&, std::initializer_list<const Expression>);
    void compile(const Expression&, std::stringstream&) const;
  };

}

#endif // __HEADCOMPILER_HPP__
