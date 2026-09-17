
#ifndef __HBOSTREAM_HPP__
#define __HBOSTREAM_HPP__


#include <cstdint>
#include "binarystream.hpp"

namespace Zigurat
{

  // HOST byte order: every read and write below is the machine's own bytes,
  // with no swap anywhere. filestream, mapstream and bufferstream are all
  // hbostream, so ANYTHING WRITTEN TO A FILE THROUGH ONE IS IN THE WRITING
  // MACHINE'S ORDER and does not cross an endian boundary -- an int64
  // written on x86-64 reads reversed on s390x, and nothing in the file says
  // which kind wrote it.
  //
  // That is deliberate, and it is the cheaper half of a pair: nbostream is
  // the other, network order with the octets reversed on a little-endian
  // host, and networkstream is built on it -- because a CONNECTION crosses
  // machines and a store does not.
  //
  // The MVCCS store keeps a byteorder.bin beside its two files for exactly
  // this reason and refuses a foreign one by name (store_order_check, in
  // MVCCS-cicili). Anything else written through an hbostream carries the
  // same rule and no such guard.
  class hbostream : public binarystream
  {
  public:
    using binarystream::binarystream;
    
  public:
    // STD Types out
    virtual int16_t read_std_short() override;
    virtual uint16_t read_std_ushort() override;
    virtual int32_t read_std_int() override;
    virtual uint32_t read_std_uint() override;
    virtual int64_t read_std_long() override;
    virtual uint64_t read_std_ulong() override;
    virtual float read_std_float() override;
    virtual double read_std_double() override;
    virtual long double read_std_real() override;
    virtual size_t read_std_size() override;
    virtual time_t read_std_time() override;
    virtual clock_t read_std_clock() override;

    // STD Types load
    virtual void read_std_short(int16_t&) override;
    virtual void read_std_ushort(uint16_t&) override;
    virtual void read_std_int(int32_t&) override;
    virtual void read_std_uint(uint32_t&) override;
    virtual void read_std_long(int64_t&) override;
    virtual void read_std_ulong(uint64_t&) override;
    virtual void read_std_float(float&) override;
    virtual void read_std_double(double&) override;
    virtual void read_std_real(long double&) override;
    virtual void read_std_size(size_t&) override;
    virtual void read_std_time(time_t&) override;
    virtual void read_std_clock(clock_t&) override;

    // STD Types rvalue reference
    virtual void write_std_short(int16_t&&) override;
    virtual void write_std_ushort(uint16_t&&) override;
    virtual void write_std_int(int32_t&&) override;
    virtual void write_std_uint(uint32_t&&) override;
    virtual void write_std_long(int64_t&&) override;
    virtual void write_std_ulong(uint64_t&&) override;
    virtual void write_std_float(float&&) override;
    virtual void write_std_double(double&&) override;
    virtual void write_std_real(long double&&) override;
    virtual void write_std_size(size_t&&) override;
    virtual void write_std_time(time_t&&) override;
    virtual void write_std_clock(clock_t&&) override;

    // STD Types lvalue reference
    virtual void write_std_short(const int16_t&) override;
    virtual void write_std_ushort(const uint16_t&) override;
    virtual void write_std_int(const int32_t&) override;
    virtual void write_std_uint(const uint32_t&) override;
    virtual void write_std_long(const int64_t&) override;
    virtual void write_std_ulong(const uint64_t&) override;
    virtual void write_std_float(const float&) override;
    virtual void write_std_double(const double&) override;
    virtual void write_std_real(const long double&) override;
    virtual void write_std_size(const size_t&) override;
    virtual void write_std_time(const time_t&) override;
    virtual void write_std_clock(const clock_t&) override;
  };

}

#endif // __HBOSTREAM_HPP__
