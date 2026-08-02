#include "nbostream.hpp"
#include <iostream>


namespace Zigurat
{

  int16_t nbostream::read_std_short()
  {
    int16_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(int16_t));
    return object;
  }

  uint16_t nbostream::read_std_ushort()
  {
    uint16_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(uint16_t));
    return object;
  }

  int32_t nbostream::read_std_int()
  {
    int32_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(int32_t));
    return object;
  }

  uint32_t nbostream::read_std_uint()
  {
    uint32_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(uint32_t));
    return object;
  }

  int64_t nbostream::read_std_long()
  {
    int64_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(int64_t));
    return object;
  }

  uint64_t nbostream::read_std_ulong()
  {
    uint64_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(uint64_t));
    return object;
  }

  float nbostream::read_std_float()
  {
    float object;
    this->read(reinterpret_cast<char*>(&object), sizeof(float));
    return object;
  }

  double nbostream::read_std_double()
  {
    double object;
    this->read(reinterpret_cast<char*>(&object), sizeof(double));
    return object;
  }

  long double nbostream::read_std_real()
  {
    long double object;
    this->read(reinterpret_cast<char*>(&object), sizeof(long double));
    return object;
  }

  size_t nbostream::read_std_size()
  {
    size_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(size_t));
    return object;
  }

  time_t nbostream::read_std_time()
  {
    time_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(time_t));
    return object;
  }

  clock_t nbostream::read_std_clock()
  {
    clock_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(clock_t));
    return object;
  }

  void nbostream::read_std_short(int16_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(int16_t));
  }

  void nbostream::read_std_ushort(uint16_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(uint16_t));
  }

  void nbostream::read_std_int(int32_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(int32_t));
  }

  void nbostream::read_std_uint(uint32_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(uint32_t));
  }

  void nbostream::read_std_long(int64_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(int64_t));
  }

  void nbostream::read_std_ulong(uint64_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(uint64_t));
  }

  void nbostream::read_std_float(float& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(float));
  }

  void nbostream::read_std_double(double& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(double));
  }

  void nbostream::read_std_real(long double& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(long double));
  }

  void nbostream::read_std_size(size_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(size_t));
  }

  void nbostream::read_std_time(time_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(time_t));
  }

  void nbostream::read_std_clock(clock_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(clock_t));
  }

  void nbostream::write_std_short(int16_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(int16_t));
  }

  void nbostream::write_std_ushort(uint16_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(uint16_t));
  }

  void nbostream::write_std_int(int32_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(int32_t));
  }

  void nbostream::write_std_uint(uint32_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(uint32_t));
  }

  void nbostream::write_std_long(int64_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(int64_t));
  }

  void nbostream::write_std_ulong(uint64_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(uint64_t));
  }

  void nbostream::write_std_float(float&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(float));
  }

  void nbostream::write_std_double(double&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(double));
  }

  void nbostream::write_std_real(long double&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(long double));
  }

  void nbostream::write_std_size(size_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(size_t));
  }

  void nbostream::write_std_time(time_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(time_t));
  }

  void nbostream::write_std_clock(clock_t&& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(clock_t));
  }

  void nbostream::write_std_short(const int16_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(int16_t));
  }

  void nbostream::write_std_ushort(const uint16_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(uint16_t));
  }

  void nbostream::write_std_int(const int32_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(int32_t));
  }

  void nbostream::write_std_uint(const uint32_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(uint32_t));
  }

  void nbostream::write_std_long(const int64_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(int64_t));
  }

  void nbostream::write_std_ulong(const uint64_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(uint64_t));
  }

  void nbostream::write_std_float(const float& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(float));
  }

  void nbostream::write_std_double(const double& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(double));
  }

  void nbostream::write_std_real(const long double& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(long double));
  }

  void nbostream::write_std_size(const size_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(size_t));
  }

  void nbostream::write_std_time(const time_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(time_t));
  }

  void nbostream::write_std_clock(const clock_t& object)
  {
    this->write(reinterpret_cast<const char*>(&object), sizeof(clock_t));
  }

}
