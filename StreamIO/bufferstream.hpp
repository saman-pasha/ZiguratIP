
#ifndef __BUFFERSTREAM_HPP__
#define __BUFFERSTREAM_HPP__

#include "hbostream.hpp"
#include <sstream>

namespace Zigurat
{

  class bufferstream : public hbostream
  {
  public:
    using hbostream::hbostream;

    bufferstream();
    explicit bufferstream(std::ios_base::openmode);
    explicit bufferstream(std::basic_string<char>, std::ios_base::openmode = std::ios_base::in | std::ios_base::out);

    bufferstream& operator=(const bufferstream&) = delete;
    bufferstream& operator=(bufferstream&&);

    std::basic_string<char> string();
    void string(std::basic_string<char>);

    void swap(bufferstream&);

    virtual ~bufferstream();
  };

}

#endif // __BUFFERSTREAM_HPP__
