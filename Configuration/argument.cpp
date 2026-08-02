#include "argument.hpp"
#include "argumentexception.hpp"
#include "utility.hpp"
#include <algorithm>


namespace Zigurat
{

  Argument::Argument(int argc, char* argv[])
  {
    std::string name;
    for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
	name = argv[i];
	if (name.find('=') != std::string::npos) {
	  std::vector<std::string> parts = Utility::split(name, '=');
	  this->_args[parts[0]].push_back(parts[1]);
	  this->_flags.push_back(parts[0]);	  
	  name.clear();
	} else {
	  this->_flags.push_back(name);	  
	}
      } else {
	this->_args[name].push_back(argv[i]);
      }
    }
  }

  bool Argument::flag(std::string name)
  {
    return (std::find(this->_flags.begin(), this->_flags.end(), name) != this->_flags.end());
  }

  bool Argument::get(std::string name, std::string& value)
  {
    if (this->_args.find(name) != this->_args.end()) {
      value = this->_args[name].front();
      return true;
    }
    return false;
  }

  bool Argument::get(std::string name, std::vector<std::string>& value)
  {
    if (this->_args.find(name) != this->_args.end()) {
      for (const std::string& part : this->_args[name])
	value.push_back(part);
      return true;
    }
    return false;
  }

  std::string Argument::get(std::string name)
  {
    if (this->_args.find(name) != this->_args.end()) {
      return this->_args[name].front();
    }
    return "";
  }

}

