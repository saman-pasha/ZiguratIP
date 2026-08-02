
#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <iostream>
#include <sstream>
#include <initializer_list>

namespace Zigurat
{

  class Buffer          // Octet String Buffer
  {
  protected:
    std::stringstream _stream;
    size_t _length(std::istream&);

  public:
    Buffer() = default;
    Buffer(const char*, size_t);
    Buffer(const uint8_t*, size_t);
    Buffer(std::initializer_list<int>);
    Buffer(const std::string&);
    Buffer(std::string&&);
    Buffer(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;    
    std::stringstream& sstream();
    size_t length();
    uint8_t peek();
    uint8_t get();
    uint8_t at(std::streampos);
    size_t read(char*, size_t);
    size_t read(uint8_t*, size_t);
    size_t read(std::ostream&, size_t);
    size_t read(Buffer&, size_t);
    size_t read_line(Buffer&, uint8_t = '\n');
    size_t dump(std::ostream&);
    size_t dump(Buffer&);
    std::streampos tell_read();
    void seek_read(std::streamoff, std::ios::seekdir);
    bool eof() const;
    void put(char);
    void put(uint8_t);
    void put(int);
    void put(unsigned int);
    void write(const char*, size_t);
    void write(const std::string&);
    void write(std::string&&);
    void write(const uint8_t*, size_t);
    void write(std::istream&, size_t);
    void write(Buffer&, size_t);
    void load(std::istream&);
    void load(Buffer&);
    std::streampos tell_write();
    void seek_write(std::streamoff, std::ios::seekdir);
    void reset();
    void clear();
    void string(std::string);
    std::string string();

    template <typename T> Buffer& operator>>(T& value)
    {
      this->_stream >> value;
      return *this;
    }

    template <typename T> Buffer& operator<<(T&& value)
    {
      this->_stream << value;
      return *this;
    }

    template <typename T> Buffer& operator<<(const T& value)
    {
      this->_stream << value;
      return *this;
    }
    
    friend std::ostream& operator<<(std::ostream&, Buffer&);
  };
  
}

#endif // __BUFFER_H__
