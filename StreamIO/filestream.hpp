
#ifndef __FILESTREAM_HPP__
#define __FILESTREAM_HPP__

#include "hbostream.hpp"
#include <fstream>

namespace Zigurat
{

  class filestream : public hbostream
  {
  public:
    using hbostream::hbostream;

    filestream();
    explicit filestream(std::basic_string<char>, std::ios_base::openmode = std::ios_base::in | std::ios_base::out);

    filestream& operator=(const filestream&) = delete;
    filestream& operator=(filestream&&);

    void open(std::basic_string<char>, std::ios_base::openmode = std::ios_base::in | std::ios_base::out);
    bool is_open() const;
    void close();

    void swap(filestream&);

    virtual ~filestream();
  };

}

#endif // __FILESTREAM_HPP__
