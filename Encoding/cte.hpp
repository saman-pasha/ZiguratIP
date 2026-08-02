
#ifndef __CTE_HPP__
#define __CTE_HPP__

#include "binarystream.hpp"

namespace Zigurat
{

  class CTE         // Content-Transfer-Encoding
  {
  public:
    enum Scheme {
      HEX,
      BASE16,
      BASE32,
      BASE32HEX,
      BASE64,
      BASE64URL,
      MIME,
      PEM
    };

  public:    
    CTE() = delete;
    static void encode(Scheme, binarystream&, size_t, binarystream&);
    static void decode(Scheme, binarystream&, size_t, binarystream&);
  };

}

#endif // __CTE_HPP__
