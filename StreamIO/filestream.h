
#ifndef __FILESTREAM_H__
#define __FILESTREAM_H__

#include "hbostream.h"
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

#endif // __FILESTREAM_H__
