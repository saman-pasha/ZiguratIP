
#ifndef __FILESTREAM_HPP__
#define __FILESTREAM_HPP__

#include "hbostream.hpp"
#include <fstream>

namespace Zigurat
{

  class filestream : public hbostream
  {
  private:
    // A std::basic_filebuf keeps its descriptor to itself -- there is no
    // portable way to ask for it -- so a second one is opened on the same path
    // and kept for nothing but fsync. One extra descriptor per store file.
    int _descriptor = -1;

    void _open_descriptor(const std::basic_string<char>&);
    void _close_descriptor();

  public:
    using hbostream::hbostream;

    filestream();
    explicit filestream(std::basic_string<char>, std::ios_base::openmode = std::ios_base::in | std::ios_base::out);

    filestream& operator=(const filestream&) = delete;
    filestream& operator=(filestream&&);

    void open(std::basic_string<char>, std::ios_base::openmode = std::ios_base::in | std::ios_base::out);
    bool is_open() const;
    void close();

    // fsync on that descriptor. False if the file was never opened through
    // open(), or if the kernel refuses -- the caller is committing, so it wants
    // to know rather than to be told everything went fine.
    bool sync_to_disk() override;

    void swap(filestream&);

    virtual ~filestream();
  };

}

#endif // __FILESTREAM_HPP__
