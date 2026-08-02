
#ifndef __BASE64URL_H__
#define __BASE64URL_H__

#include "bufferstream.h"

namespace Zigurat
{

  class Base64URL
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

#endif // __BASE64URL_H__
