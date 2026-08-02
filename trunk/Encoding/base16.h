
#ifndef __BASE16_H__
#define __BASE16_H__

#include "binarystream.h"

namespace Zigurat
{

  class Base16
  {
  public:
    static const char  index[16];
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

#endif // __BASE16_H__
