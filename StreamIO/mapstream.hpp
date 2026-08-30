
#ifndef __MAPSTREAM_HPP__
#define __MAPSTREAM_HPP__

#include "hbostream.hpp"
#include "mapbuf.hpp"

namespace Zigurat
{

  // filestream's shape over a mapbuf: the same host-byte-order accessors,
  // the same open/close/is_open, sync_to_disk that means it. Where a
  // filestream is a binarystream over std::basic_filebuf, this is one over
  // a memory mapping -- see mapbuf.hpp for what that buys the store.
  class mapstream : public hbostream
  {
  public:
    using hbostream::hbostream;

    mapstream();
    explicit mapstream(std::basic_string<char>, std::ios_base::openmode = std::ios_base::in | std::ios_base::out);

    mapstream(const mapstream&) = delete;
    mapstream& operator=(const mapstream&) = delete;

    void open(std::basic_string<char>, std::ios_base::openmode = std::ios_base::in | std::ios_base::out);
    bool is_open() const;
    void close();

    // one block write, not one put per byte: a page's worth of fill is
    // one extension of the file (mapbuf.hpp)
    std::basic_ostream<char_type>& fill_n(size_t, char_type) override;
    bool sync_to_disk() override;

    virtual ~mapstream();
  };

}

#endif // __MAPSTREAM_HPP__
