
#ifndef __ARRAYSTREAM_HPP__
#define __ARRAYSTREAM_HPP__

#include "hbostream.hpp"
#include "arraybuf.hpp"

namespace Zigurat
{

  class arraystream : public hbostream
  {
  public:
    using hbostream::hbostream;
    
    arraystream(char*, size_t);
    arraystream(uint8_t*, size_t);

    arraystream& operator=(const arraystream&) = delete;
    arraystream& operator=(arraystream&&);

    void swap(arraystream&);

    virtual ~arraystream();
  };

}

#endif // __ARRAYSTREAM_HPP__
