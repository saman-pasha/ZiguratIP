#include "mapbuf.hpp"
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

namespace Zigurat
{

  // 1 GiB of address space per mapping: nothing is committed until touched,
  // and a store past it remaps once to twice the size.
  const std::size_t mapbuf::RESERVE = (std::size_t)1 << 30;

  static std::size_t page_round(std::size_t n)
  {
    static const std::size_t page = (std::size_t)::sysconf(_SC_PAGESIZE);
    return (n + page - 1) / page * page;
  }

  mapbuf::mapbuf()
    : _fd(-1), _base(nullptr), _capacity(0), _size(0), _pos(0), _writable(false)
  {
  }

  mapbuf::~mapbuf()
  {
    this->close();
  }

  mapbuf* mapbuf::open(const std::string& path, std::ios_base::openmode mode)
  {
    this->close();

    const bool out = (mode & std::ios_base::out) != 0;
    int flags = out ? (O_RDWR | O_CREAT) : O_RDONLY;
    if (mode & std::ios_base::trunc) flags |= O_TRUNC;

    int fd = ::open(path.c_str(), flags, 0644);
    if (fd < 0) return nullptr;

    struct stat st;
    if (::fstat(fd, &st) != 0) { ::close(fd); return nullptr; }

    this->_fd = fd;
    this->_writable = out;
    this->_size = (std::streamsize)st.st_size;
    this->_pos = 0;

    if (!this->reserve((std::size_t)this->_size)) {
      ::close(fd);
      this->_fd = -1;
      return nullptr;
    }
    return this;
  }

  mapbuf* mapbuf::close()
  {
    this->unmap();
    if (this->_fd >= 0) {
      ::close(this->_fd);
      this->_fd = -1;
    }
    this->_size = 0;
    this->_pos = 0;
    this->_writable = false;
    return this;
  }

  void mapbuf::unmap()
  {
    if (this->_base != nullptr) {
      ::munmap(this->_base, this->_capacity);
      this->_base = nullptr;
      this->_capacity = 0;
    }
  }

  // Maps [0, capacity) with capacity at least RESERVE and at least n, page
  // rounded. Mapping past the file's end is allowed; touching past it is
  // not, and nothing here does until extend() has moved the end.
  bool mapbuf::reserve(std::size_t n)
  {
    std::size_t capacity = page_round(n > RESERVE ? n : RESERVE);
    if (this->_base != nullptr && capacity <= this->_capacity) return true;
    // a store past the reservation doubles it, so remaps stay rare
    if (this->_base != nullptr) {
      while (capacity < n) capacity *= 2;
      if (capacity < this->_capacity * 2) capacity = this->_capacity * 2;
    }
    this->unmap();
    const int prot = PROT_READ | (this->_writable ? PROT_WRITE : 0);
    void* base = ::mmap(nullptr, capacity, prot, MAP_SHARED, this->_fd, 0);
    if (base == MAP_FAILED) return false;
    this->_base = (char*)base;
    this->_capacity = capacity;
    return true;
  }

  bool mapbuf::refresh()
  {
    if (this->_fd < 0) return false;
    struct stat st;
    if (::fstat(this->_fd, &st) != 0) return false;
    if ((std::streamsize)st.st_size != this->_size) {
      this->_size = (std::streamsize)st.st_size;
      if ((std::size_t)this->_size > this->_capacity)
        return this->reserve((std::size_t)this->_size);
    }
    return true;
  }

  bool mapbuf::extend(std::streamsize length)
  {
    if (!this->_writable || this->_fd < 0) return false;
    if (length <= this->_size) return true;
    if ((std::size_t)length > this->_capacity && !this->reserve((std::size_t)length)) return false;
    if (::ftruncate(this->_fd, (off_t)length) != 0) return false;
    this->_size = length;
    return true;
  }

  std::streamsize mapbuf::size()
  {
    if (!this->_writable) this->refresh();
    return this->_size;
  }

  bool mapbuf::sync_to_disk()
  {
    if (this->_fd < 0) return false;
    if (!this->_writable) return true;
    bool ok = true;
    if (this->_base != nullptr && this->_size > 0)
      ok = (::msync(this->_base, page_round((std::size_t)this->_size), MS_SYNC) == 0);
    return (::fsync(this->_fd) == 0) && ok;
  }

  mapbuf::pos_type mapbuf::seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode)
  {
    if (this->_fd < 0) return pos_type(off_type(-1));
    std::streamsize base = 0;
    if (dir == std::ios_base::cur) base = this->_pos;
    else if (dir == std::ios_base::end) { if (!this->_writable) this->refresh(); base = this->_size; }
    const std::streamsize target = base + (std::streamsize)off;
    if (target < 0) return pos_type(off_type(-1));
    this->_pos = target;
    return pos_type(off_type(target));
  }

  mapbuf::pos_type mapbuf::seekpos(pos_type pos, std::ios_base::openmode which)
  {
    return this->seekoff(off_type(pos), std::ios_base::beg, which);
  }

  std::streamsize mapbuf::xsgetn(char* s, std::streamsize n)
  {
    if (this->_fd < 0 || n <= 0) return 0;
    if (this->_pos + n > this->_size && !this->_writable) this->refresh();
    std::streamsize avail = this->_size - this->_pos;
    if (avail <= 0) return 0;
    if (avail > n) avail = n;
    std::memcpy(s, this->_base + this->_pos, (std::size_t)avail);
    this->_pos += avail;
    return avail;
  }

  std::streamsize mapbuf::xsputn(const char* s, std::streamsize n)
  {
    if (this->_fd < 0 || !this->_writable || n <= 0) return 0;
    if (this->_pos + n > this->_size && !this->extend(this->_pos + n)) return 0;
    std::memcpy(this->_base + this->_pos, s, (std::size_t)n);
    this->_pos += n;
    return n;
  }

  mapbuf::int_type mapbuf::underflow()
  {
    if (this->_fd < 0) return traits_type::eof();
    if (this->_pos >= this->_size && !this->_writable) this->refresh();
    if (this->_pos >= this->_size) return traits_type::eof();
    return traits_type::to_int_type(this->_base[this->_pos]);
  }

  mapbuf::int_type mapbuf::uflow()
  {
    int_type c = this->underflow();
    if (c != traits_type::eof()) this->_pos++;
    return c;
  }

  mapbuf::int_type mapbuf::overflow(int_type c)
  {
    if (traits_type::eq_int_type(c, traits_type::eof())) return traits_type::not_eof(c);
    if (this->_fd < 0 || !this->_writable) return traits_type::eof();
    if (this->_pos + 1 > this->_size && !this->extend(this->_pos + 1)) return traits_type::eof();
    this->_base[this->_pos++] = traits_type::to_char_type(c);
    return c;
  }

  mapbuf::int_type mapbuf::pbackfail(int_type c)
  {
    if (this->_fd < 0 || this->_pos <= 0) return traits_type::eof();
    this->_pos--;
    if (!traits_type::eq_int_type(c, traits_type::eof())) {
      if (this->_writable) this->_base[this->_pos] = traits_type::to_char_type(c);
      else if (!traits_type::eq(this->_base[this->_pos], traits_type::to_char_type(c)))
        { this->_pos++; return traits_type::eof(); }
    }
    return traits_type::not_eof(c);
  }

  std::streamsize mapbuf::showmanyc()
  {
    if (this->_fd < 0) return -1;
    if (!this->_writable) this->refresh();
    return this->_size - this->_pos;
  }

}
