
#ifndef __SHAHELPER_H__
#define __SHAHELPER_H__

#include <iostream>
#include <string>

namespace Zigurat
{

  class SHA
  {
  public:
    enum Version : int
    {
      SHA1   = 1, 
      SHA224 = 224, 
      SHA256 = 256, 
      SHA384 = 384, 
      SHA512 = 512
    };

    static void        checksum(Version, const uint8_t*, size_t, uint8_t*);
    static void        checksum(Version, std::istream&, size_t, std::ostream&);
    static std::string checksum(Version, const std::string&);

    static void        hmac(Version, const uint8_t*, size_t, const uint8_t*, size_t, uint8_t*);
    static void        hmac(Version, std::istream&, size_t, std::istream&, size_t, std::ostream&);

    static size_t      size(Version);
    static std::string name(Version);
  };

}

#endif // __SHAHELPER_H__
