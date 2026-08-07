
#ifndef __BASE64_HPP__
#define __BASE64_HPP__


#include <cstdint>
#include "bufferstream.hpp"

namespace Zigurat
{

  class Base64
  {
  public:
    static const char  index[64];
    static void        encode(const uint8_t*, size_t, uint8_t*);
    static void        encode(binarystream&, size_t, binarystream&);
    static std::string encode(const std::string&);
    static size_t      encode_size(size_t);
    static void        decode(const uint8_t*, size_t, uint8_t*);
    static void        decode(binarystream&, size_t, binarystream&);
    static std::string decode(const std::string&);
    static size_t      decode_size(size_t);
  };

}

#endif // __BASE64_HPP__
