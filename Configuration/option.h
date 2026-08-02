
#ifndef __OPTION_H__
#define __OPTION_H__

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

#endif // __OPTION_H__

