
#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

#include <iostream>
#include <string>
#include <tuple>
#include <list>
#include "option.h"

namespace Zigurat
{

  class Option;

  class Configuration
  {
  public:
    Configuration();
    Configuration(std::istream&);
    Configuration(const std::string);
    Option root() const;
    bool get(const std::string, const Option*) const;
    bool get(const Option&, const std::string, const Option*) const;
    bool get(const std::string, std::string&) const;
    bool get(const Option&, const std::string, std::string&) const;
    bool childs(const std::string, std::list<std::string>&) const;
    bool childs(const Option&, const std::string, std::list<std::string>&) const;
    bool childs(const std::string, std::list<const Option*>&) const;
    bool childs(const Option&, const std::string, std::list<const Option*>&) const;
    bool list(const std::string, std::list<std::string>&) const;
    bool list(const Option&, const std::string, std::list<std::string>&) const;
    bool list(const std::string, std::list<const Option*>&) const;
    bool list(const Option&, const std::string, std::list<const Option*>&) const;
    bool extract(const std::string, std::list<std::string>&) const;
    bool extract(const Option&, const std::string, std::list<std::string>&) const;
    bool extract(const std::string, std::list<const Option*>&) const;
    bool extract(const Option&, const std::string, std::list<const Option*>&) const;
    void load(std::istream&);
    void load(const std::string);
    void print() const;
  private:
    std::istream* _file;
    Option _root;
    void _get_part(std::stringstream&, std::string&, char) const;
    bool _childs(const Option&, const std::string&, std::list<const Option*>&) const;
    bool _list(const Option&, const std::string&, std::list<const Option*>&) const;
    bool _extract(const Option&, const std::string&, std::list<const Option*>&) const;
    void _attr_key(std::string&, std::string&) const;
    void _load(Option&, int&, int);
    void _print(const Option&, int) const;
    std::tuple<int, std::string, std::string> _split(std::string, int, int);
  };

}

#endif // __CONFIGURATION_H__

