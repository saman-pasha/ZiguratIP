
#ifndef __SHAHELPER_HPP__
#define __SHAHELPER_HPP__


#include <cstdint>
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

    // HMAC(key, message), in that order -- the order it is written in RFC 2104
    // and everywhere else. It used to take the message first, following the
    // C interface underneath, and callers reading "hmac(SHA256, secret, ...)"
    // naturally passed the key there instead. HMAC(k, m) is not HMAC(m, k), so
    // those callers computed something else entirely.
    static void        hmac(Version, const uint8_t* key, size_t, const uint8_t* text, size_t, uint8_t*);
    static void        hmac(Version, std::istream& key, size_t, std::istream& text, size_t, std::ostream&);

    static size_t      size(Version);
    static std::string name(Version);
  };

}

#endif // __SHAHELPER_HPP__
