#include "networkstream.hpp"


namespace Zigurat
{

  networkstream::networkstream()
    : nbostream(new std::basic_stringbuf<char>())
  {

  }

  networkstream::networkstream(std::ios_base::openmode mode)
    : nbostream(new std::basic_stringbuf<char>(mode))
  {

  }
  
  networkstream::networkstream(std::basic_string<char> string, std::ios_base::openmode mode)
    : nbostream(new std::basic_stringbuf<char>(string, mode))
  {

  }

  networkstream& networkstream::operator=(networkstream&& other)
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();

    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);

    return *this;
  }

  std::basic_string<char> networkstream::string()
  {
    return dynamic_cast<std::basic_stringbuf<char>*>(this->rdbuf())->str();
  }

  void networkstream::string(std::basic_string<char> string)
  {
    dynamic_cast<std::basic_stringbuf<char>*>(this->rdbuf())->str(string);
  }

  void networkstream::swap(networkstream& other)
  {
    std::basic_streambuf<char>* tmp = other.rdbuf();
    other.rdbuf(this->rdbuf());
    this->rdbuf(tmp);
  }

  networkstream::~networkstream()
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();
  }

}
