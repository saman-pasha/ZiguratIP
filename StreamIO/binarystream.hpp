
#ifndef __BINARYSTREAM_HPP__
#define __BINARYSTREAM_HPP__

#include <iostream>

namespace Zigurat
{

  class binarystream : public std::basic_iostream<char>
  {
  public:
    using std::basic_iostream<char>::basic_iostream;
    using std::basic_iostream<char>::read;
    using std::basic_iostream<char>::write;
    using std::basic_iostream<char>::getline;
    
    virtual std::streamsize length();
    virtual char_type at(std::streampos);
    virtual std::basic_istream<char_type>& read(std::ostream&, std::streamsize);
    virtual std::basic_istream<char_type>& read(char_type*, std::streampos, std::streamsize);
    virtual std::basic_istream<char_type>& read(std::ostream&, std::streampos, std::streamsize);
    virtual std::basic_istream<char_type>& getline(std::ostream&, char_type = '\n');
    virtual std::basic_istream<char_type>& getline(std::ostream&, int, char_type = '\n');
    virtual std::basic_istream<char_type>& getline(std::ostream&, std::streamsize, char_type = '\n');
    virtual std::streamsize read_exact(char_type*, std::streamsize);
    virtual std::streamsize read_exact(std::ostream&, std::streamsize);
    virtual std::basic_ostream<char_type>& write(std::istream&, std::streamsize);
    virtual std::basic_ostream<char_type>& write(const char_type*, std::streamsize, std::streampos);
    virtual std::basic_ostream<char_type>& write(std::istream&, std::streamsize, std::streampos);
    virtual std::basic_ostream<char_type>& fill_n(size_t, char_type);

    // STD Types out
    virtual bool read_std_bool();
    virtual char read_std_char();
    virtual int8_t read_std_byte();
    virtual uint8_t read_std_ubyte();
    virtual int16_t read_std_short() = 0;
    virtual uint16_t read_std_ushort() = 0;
    virtual int32_t read_std_int() = 0;
    virtual uint32_t read_std_uint() = 0;
    virtual int64_t read_std_long() = 0;
    virtual uint64_t read_std_ulong() = 0;
    virtual float read_std_float() = 0;
    virtual double read_std_double() = 0;
    virtual long double read_std_real() = 0;
    virtual size_t read_std_size() = 0;
    virtual time_t read_std_time() = 0;
    virtual clock_t read_std_clock() = 0;
    virtual std::string read_std_string();
    virtual std::string read_std_text();

    // STD Types load
    virtual void read_std_bool(bool&);
    virtual void read_std_char(char&);
    virtual void read_std_byte(int8_t&);
    virtual void read_std_ubyte(uint8_t&);
    virtual void read_std_short(int16_t&) = 0;
    virtual void read_std_ushort(uint16_t&) = 0;
    virtual void read_std_int(int32_t&) = 0;
    virtual void read_std_uint(uint32_t&) = 0;
    virtual void read_std_long(int64_t&) = 0;
    virtual void read_std_ulong(uint64_t&) = 0;
    virtual void read_std_float(float&) = 0;
    virtual void read_std_double(double&) = 0;
    virtual void read_std_real(long double&) = 0;
    virtual void read_std_size(size_t&) = 0;
    virtual void read_std_time(time_t&) = 0;
    virtual void read_std_clock(clock_t&) = 0;
    virtual void read_std_string(std::string&);
    virtual void read_std_text(std::string&);

    // STD Types rvalue reference
    virtual void write_std_bool(bool&&);
    virtual void write_std_char(char&&);
    virtual void write_std_byte(int8_t&&);
    virtual void write_std_ubyte(uint8_t&&);
    virtual void write_std_short(int16_t&&) = 0;
    virtual void write_std_ushort(uint16_t&&) = 0;
    virtual void write_std_int(int32_t&&) = 0;
    virtual void write_std_uint(uint32_t&&) = 0;
    virtual void write_std_long(int64_t&&) = 0;
    virtual void write_std_ulong(uint64_t&&) = 0;
    virtual void write_std_float(float&&) = 0;
    virtual void write_std_double(double&&) = 0;
    virtual void write_std_real(long double&&) = 0;
    virtual void write_std_size(size_t&&) = 0;
    virtual void write_std_time(time_t&&) = 0;
    virtual void write_std_clock(clock_t&&) = 0;
    virtual void write_std_string(std::string&&);
    virtual void write_std_text(std::string&&);

    // STD Types lvalue reference
    virtual void write_std_bool(const bool&);
    virtual void write_std_char(const char&);
    virtual void write_std_byte(const int8_t&);
    virtual void write_std_ubyte(const uint8_t&);
    virtual void write_std_short(const int16_t&) = 0;
    virtual void write_std_ushort(const uint16_t&) = 0;
    virtual void write_std_int(const int32_t&) = 0;
    virtual void write_std_uint(const uint32_t&) = 0;
    virtual void write_std_long(const int64_t&) = 0;
    virtual void write_std_ulong(const uint64_t&) = 0;
    virtual void write_std_float(const float&) = 0;
    virtual void write_std_double(const double&) = 0;
    virtual void write_std_real(const long double&) = 0;
    virtual void write_std_size(const size_t&) = 0;
    virtual void write_std_time(const time_t&) = 0;
    virtual void write_std_clock(const clock_t&) = 0;
    virtual void write_std_string(const std::string&);
    virtual void write_std_text(const std::string&);

    template <typename F> static int64_t pack_size(const F&);
    template <typename F, typename... Rs> static int64_t pack_size(const F&, const Rs&...);

    template <typename F> void pack(const F&);
    template <typename F, typename... Rs> void pack(const F&, const Rs&...);

    template <typename F> void unpack(F&);
    template <typename F, typename... Rs> void unpack(F&, Rs&...);
  };

  template <typename F> 
  int64_t binarystream::pack_size(const F& first)
  {
    return first.pack_size();
  }

  template <typename F, typename... Rs> 
  int64_t binarystream::pack_size(const F& first, const Rs&... rest)
  {
    return first.pack_size() + binarystream::pack_size(rest...);
  }

  template <typename F> 
  void binarystream::pack(const F& first)
  {
    *this << first;
  }

  template <typename F, typename... Rs> 
  void binarystream::pack(const F& first, const Rs&... rest)
  {
    *this << first;
    this->pack(rest...);
  }

  template <typename F> 
  void binarystream::unpack(F& first)
  {
    *this >> first;
  }

  template <typename F, typename... Rs> 
  void binarystream::unpack(F& first, Rs&... rest)
  {
    *this >> first;
    this->unpack(rest...);
  }

}

#endif // __BINARYSTREAM_HPP__
