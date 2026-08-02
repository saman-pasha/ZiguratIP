#include "bufferstream.h"


namespace Zigurat
{

  bufferstream::bufferstream()
    : hbostream(new std::basic_stringbuf<char>())
  {

  }

  bufferstream::bufferstream(std::ios_base::openmode mode)
    : hbostream(new std::basic_stringbuf<char>(mode))
  {

  }
  
  bufferstream::bufferstream(std::basic_string<char> string, std::ios_base::openmode mode)
    : hbostream(new std::basic_stringbuf<char>(string, mode))
  {

  }

  bufferstream& bufferstream::operator=(bufferstream&& other)
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();

    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);

    return *this;
  }

  std::basic_string<char> bufferstream::string()
  {
    return dynamic_cast<std::basic_stringbuf<char>*>(this->rdbuf())->str();
  }

  void bufferstream::string(std::basic_string<char> string)
  {
    dynamic_cast<std::basic_stringbuf<char>*>(this->rdbuf())->str(string);
  }

  void bufferstream::swap(bufferstream& other)
  {
    std::basic_streambuf<char>* tmp = other.rdbuf();
    other.rdbuf(this->rdbuf());
    this->rdbuf(tmp);
  }

  bufferstream::~bufferstream()
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();
  }

}
