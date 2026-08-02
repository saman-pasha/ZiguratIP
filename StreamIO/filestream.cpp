#include "filestream.h"


namespace Zigurat
{

  filestream::filestream()
    : hbostream(new std::basic_filebuf<char>())
  {

  }

  filestream::filestream(std::basic_string<char> filename, std::ios_base::openmode mode)
    : hbostream(new std::basic_filebuf<char>())
  {
    dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf())->open(filename, mode);
  }

  filestream& filestream::operator=(filestream&& other)
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();

    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);

    return *this;
  }

  void filestream::open(std::basic_string<char> filename, std::ios_base::openmode mode)
  {
    dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf())->close();
    dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf())->open(filename, mode);
  }
  
  bool filestream::is_open() const
  {
    return dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf())->is_open();
  }
  
  void filestream::close()
  {
    dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf())->close();
  }

  void filestream::swap(filestream& other)
  {
    std::basic_streambuf<char>* tmp = other.rdbuf();
    other.rdbuf(this->rdbuf());
    this->rdbuf(tmp);
  }

  filestream::~filestream()
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();
  }

}
