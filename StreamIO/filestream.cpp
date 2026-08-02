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
    if (dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf())->open(filename, mode) == nullptr)
      this->setstate(std::ios_base::failbit);
  }

  filestream& filestream::operator=(filestream&& other)
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();

    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);

    return *this;
  }

  // std::fstream::open sets failbit when the buffer cannot be opened; going
  // straight to the filebuf skipped that, so good() kept reporting success on a
  // file that was never opened and callers checking it were none the wiser.
  void filestream::open(std::basic_string<char> filename, std::ios_base::openmode mode)
  {
    std::basic_filebuf<char>* buffer = dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf());
    if (buffer == nullptr) {
      this->setstate(std::ios_base::failbit);
      return;
    }

    buffer->close();
    if (buffer->open(filename, mode) == nullptr)
      this->setstate(std::ios_base::failbit);
    else
      this->clear();
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
