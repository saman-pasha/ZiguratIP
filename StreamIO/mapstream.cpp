#include "mapstream.hpp"
#include <vector>

namespace Zigurat
{

  static mapbuf* buffer_of(std::basic_streambuf<char>* buf)
  {
    return dynamic_cast<mapbuf*>(buf);
  }

  mapstream::mapstream()
    : hbostream(new mapbuf())
  {
  }

  mapstream::mapstream(std::basic_string<char> filename, std::ios_base::openmode mode)
    : hbostream(new mapbuf())
  {
    if (buffer_of(this->rdbuf())->open(filename, mode) == nullptr)
      this->setstate(std::ios_base::failbit);
  }

  void mapstream::open(std::basic_string<char> filename, std::ios_base::openmode mode)
  {
    mapbuf* buffer = buffer_of(this->rdbuf());
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

  bool mapstream::is_open() const
  {
    mapbuf* buffer = buffer_of(this->rdbuf());
    return buffer != nullptr && buffer->is_open();
  }

  void mapstream::close()
  {
    mapbuf* buffer = buffer_of(this->rdbuf());
    if (buffer != nullptr) buffer->close();
  }

  std::basic_ostream<mapstream::char_type>& mapstream::fill_n(size_t n, char_type val)
  {
    if (n == 0) return *this;
    // bounded scratch: a page at a time, so a fill of any size is a few
    // block writes and never a large allocation
    const size_t block = n < 65536 ? n : 65536;
    std::vector<char_type> scratch(block, val);
    while (n > 0) {
      const size_t take = n < block ? n : block;
      this->write(scratch.data(), (std::streamsize)take);
      if (!this->good()) break;
      n -= take;
    }
    return *this;
  }

  bool mapstream::sync_to_disk()
  {
    mapbuf* buffer = buffer_of(this->rdbuf());
    return buffer != nullptr && buffer->sync_to_disk();
  }

  mapstream::~mapstream()
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();
  }

}
