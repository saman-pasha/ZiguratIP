
#ifndef __OPTION_HPP__
#define __OPTION_HPP__

#include <string>
#include <list>

namespace Zigurat
{

  class Option
  {
  public:
    Option() = default;
    Option(std::string, std::string, std::string);
    std::string attr;
    std::string key;
    std::string value;
    std::list<Option> options;
    virtual ~Option();	
  };

}

#endif // __OPTION_HPP__

