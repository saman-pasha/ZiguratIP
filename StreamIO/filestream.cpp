#include "filestream.hpp"
#include <fcntl.h>
#include <unistd.h>


namespace Zigurat
{

  void filestream::_open_descriptor(const std::basic_string<char>& filename)
  {
    this->_close_descriptor();

    // Read only is enough: fsync flushes the file, not this descriptor's
    // writes, and asking for write access on a store opened read only would
    // fail for no gain.
    this->_descriptor = ::open(filename.c_str(), O_RDONLY);
  }

  void filestream::_close_descriptor()
  {
    if (this->_descriptor > -1) {
      ::close(this->_descriptor);
      this->_descriptor = -1;
    }
  }

  bool filestream::sync_to_disk()
  {
    if (this->_descriptor < 0) return false;
    return (::fsync(this->_descriptor) == 0);
  }

  filestream::filestream()
    : hbostream(new std::basic_filebuf<char>())
  {

  }

  filestream::filestream(std::basic_string<char> filename, std::ios_base::openmode mode)
    : hbostream(new std::basic_filebuf<char>())
  {
    if (dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf())->open(filename, mode) == nullptr)
      this->setstate(std::ios_base::failbit);
    else
      this->_open_descriptor(filename);
  }

  filestream& filestream::operator=(filestream&& other)
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();

    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);

    this->_close_descriptor();
    this->_descriptor = other._descriptor;
    other._descriptor = -1;

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
    if (buffer->open(filename, mode) == nullptr) {
      this->setstate(std::ios_base::failbit);
    } else {
      this->clear();
      this->_open_descriptor(filename);
    }
  }

  bool filestream::is_open() const
  {
    return dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf())->is_open();
  }
  
  void filestream::close()
  {
    dynamic_cast<std::basic_filebuf<char>*>(this->rdbuf())->close();
    this->_close_descriptor();
  }

  void filestream::swap(filestream& other)
  {
    std::basic_streambuf<char>* tmp = other.rdbuf();
    other.rdbuf(this->rdbuf());
    this->rdbuf(tmp);

    int tmp_descriptor = other._descriptor;
    other._descriptor = this->_descriptor;
    this->_descriptor = tmp_descriptor;
  }

  filestream::~filestream()
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();
    this->_close_descriptor();
  }

}
