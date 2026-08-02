
#ifndef __ZLIB_H__
#define __ZLIB_H__

#include <iostream>

namespace Zigurat
{

  class ZLib
  {
  protected:
    static const int CHUNK;
    static int  def(std::istream&, std::streamsize, std::ostream&, int);
    static int  inf(std::istream&, std::streamsize, std::ostream&);
    static void zerr(int);

  public:
    enum Algorithm {
      ZLIB,
      DEFLATE,
      GZIP
    };

    static void   compress(Algorithm, std::istream&, std::streamsize, std::ostream&, int = -1);
    static void decompress(Algorithm, std::istream&, std::streamsize, std::ostream&);
  };

}

#endif // __ZLIB_H__
