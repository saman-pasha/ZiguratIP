#include "nbostream.hpp"
#include <algorithm>
#include <cstdint>


namespace Zigurat
{

  // Network byte order: most significant octet first, whatever this machine
  // prefers.
  //
  // Every method below used to be a straight copy of the object's own bytes --
  // byte for byte what hbostream does, so the two classes were the same class
  // under two names and "network byte order" was a label with nothing behind it.
  // Both ends of a ZiguratIP connection shared the assumption and therefore
  // agreed, which is why it went unnoticed until something else had to read the
  // wire: openssl took a two octet length of 00 02 for 512.
  //
  // The swap is on the whole object rather than through htons and friends so
  // that the floating point types and the ones whose width varies by platform
  // are handled by the same rule. A long double is reversed across its full
  // storage, padding included, which is consistent between two machines of the
  // same layout -- and that type was never portable across differing ones.
  namespace
  {
    bool host_is_big_endian()
    {
      const uint16_t probe = 0x0001;
      return *reinterpret_cast<const uint8_t*>(&probe) == 0x00;
    }

    template <typename T>
    T reorder(T object)
    {
      static const bool swapping = !host_is_big_endian();
      if (swapping) {
        uint8_t* octets = reinterpret_cast<uint8_t*>(&object);
        std::reverse(octets, octets + sizeof(T));
      }
      return object;
    }
  }

  // STD Types out
  int16_t nbostream::read_std_short()
  {
    int16_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(int16_t));
    return reorder(object);
  }

  uint16_t nbostream::read_std_ushort()
  {
    uint16_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(uint16_t));
    return reorder(object);
  }

  int32_t nbostream::read_std_int()
  {
    int32_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(int32_t));
    return reorder(object);
  }

  uint32_t nbostream::read_std_uint()
  {
    uint32_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(uint32_t));
    return reorder(object);
  }

  int64_t nbostream::read_std_long()
  {
    int64_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(int64_t));
    return reorder(object);
  }

  uint64_t nbostream::read_std_ulong()
  {
    uint64_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(uint64_t));
    return reorder(object);
  }

  float nbostream::read_std_float()
  {
    float object;
    this->read(reinterpret_cast<char*>(&object), sizeof(float));
    return reorder(object);
  }

  double nbostream::read_std_double()
  {
    double object;
    this->read(reinterpret_cast<char*>(&object), sizeof(double));
    return reorder(object);
  }

  long double nbostream::read_std_real()
  {
    long double object;
    this->read(reinterpret_cast<char*>(&object), sizeof(long double));
    return reorder(object);
  }

  size_t nbostream::read_std_size()
  {
    size_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(size_t));
    return reorder(object);
  }

  time_t nbostream::read_std_time()
  {
    time_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(time_t));
    return reorder(object);
  }

  clock_t nbostream::read_std_clock()
  {
    clock_t object;
    this->read(reinterpret_cast<char*>(&object), sizeof(clock_t));
    return reorder(object);
  }

  // STD Types load
  void nbostream::read_std_short(int16_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(int16_t));
    object = reorder(object);
  }

  void nbostream::read_std_ushort(uint16_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(uint16_t));
    object = reorder(object);
  }

  void nbostream::read_std_int(int32_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(int32_t));
    object = reorder(object);
  }

  void nbostream::read_std_uint(uint32_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(uint32_t));
    object = reorder(object);
  }

  void nbostream::read_std_long(int64_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(int64_t));
    object = reorder(object);
  }

  void nbostream::read_std_ulong(uint64_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(uint64_t));
    object = reorder(object);
  }

  void nbostream::read_std_float(float& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(float));
    object = reorder(object);
  }

  void nbostream::read_std_double(double& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(double));
    object = reorder(object);
  }

  void nbostream::read_std_real(long double& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(long double));
    object = reorder(object);
  }

  void nbostream::read_std_size(size_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(size_t));
    object = reorder(object);
  }

  void nbostream::read_std_time(time_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(time_t));
    object = reorder(object);
  }

  void nbostream::read_std_clock(clock_t& object)
  {
    this->read(reinterpret_cast<char*>(&object), sizeof(clock_t));
    object = reorder(object);
  }

  // STD Types in
  void nbostream::write_std_short(int16_t&& object)
  {
    const int16_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(int16_t));
  }

  void nbostream::write_std_ushort(uint16_t&& object)
  {
    const uint16_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(uint16_t));
  }

  void nbostream::write_std_int(int32_t&& object)
  {
    const int32_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(int32_t));
  }

  void nbostream::write_std_uint(uint32_t&& object)
  {
    const uint32_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(uint32_t));
  }

  void nbostream::write_std_long(int64_t&& object)
  {
    const int64_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(int64_t));
  }

  void nbostream::write_std_ulong(uint64_t&& object)
  {
    const uint64_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(uint64_t));
  }

  void nbostream::write_std_float(float&& object)
  {
    const float ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(float));
  }

  void nbostream::write_std_double(double&& object)
  {
    const double ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(double));
  }

  void nbostream::write_std_real(long double&& object)
  {
    const long double ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(long double));
  }

  void nbostream::write_std_size(size_t&& object)
  {
    const size_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(size_t));
  }

  void nbostream::write_std_time(time_t&& object)
  {
    const time_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(time_t));
  }

  void nbostream::write_std_clock(clock_t&& object)
  {
    const clock_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(clock_t));
  }

  // STD Types store
  void nbostream::write_std_short(const int16_t& object)
  {
    const int16_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(int16_t));
  }

  void nbostream::write_std_ushort(const uint16_t& object)
  {
    const uint16_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(uint16_t));
  }

  void nbostream::write_std_int(const int32_t& object)
  {
    const int32_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(int32_t));
  }

  void nbostream::write_std_uint(const uint32_t& object)
  {
    const uint32_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(uint32_t));
  }

  void nbostream::write_std_long(const int64_t& object)
  {
    const int64_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(int64_t));
  }

  void nbostream::write_std_ulong(const uint64_t& object)
  {
    const uint64_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(uint64_t));
  }

  void nbostream::write_std_float(const float& object)
  {
    const float ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(float));
  }

  void nbostream::write_std_double(const double& object)
  {
    const double ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(double));
  }

  void nbostream::write_std_real(const long double& object)
  {
    const long double ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(long double));
  }

  void nbostream::write_std_size(const size_t& object)
  {
    const size_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(size_t));
  }

  void nbostream::write_std_time(const time_t& object)
  {
    const time_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(time_t));
  }

  void nbostream::write_std_clock(const clock_t& object)
  {
    const clock_t ordered = reorder(object);
    this->write(reinterpret_cast<const char*>(&ordered), sizeof(clock_t));
  }

}
