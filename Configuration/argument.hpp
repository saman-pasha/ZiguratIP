
#ifndef __ARGUMENT_HPP__
#define __ARGUMENT_HPP__

#include <string>
#include <vector>
#include <map>

namespace Zigurat
{

  class Argument
  {
  public:
    Argument(int, char*[]);
    bool flag(std::string);
    bool get(std::string, std::string&);
    bool get(std::string, std::vector<std::string>&);
    std::string get(std::string);
  private:
    std::vector<std::string> _flags;
    std::map<std::string, std::vector<std::string> > _args;
  };

}

#endif // __ARGUMENT_HPP__

