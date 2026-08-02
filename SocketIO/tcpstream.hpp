
#ifndef __TCPSTREAM_HPP__
#define __TCPSTREAM_HPP__

#include "nbostream.hpp"
#include "tcpbuf.hpp"

namespace Zigurat
{

  class tcpstream : public nbostream
  {
  public:
    using nbostream::nbostream;
    
    tcpstream();
    explicit tcpstream(Socket::handle_t, bool = true, int = 0);
    explicit tcpstream(std::string, std::string, bool = true, int = 0);

    tcpstream(const tcpstream&) = delete;
    tcpstream(tcpstream&&);

    tcpstream& operator=(const tcpstream&) = delete;
    tcpstream& operator=(tcpstream&&);

    void open(Socket::handle_t, bool = true, int = 0);
    void open(std::string, std::string, bool = true, int = 0);
    bool is_open() const;
    void close();

    void swap(tcpstream&);

    virtual ~tcpstream();
  };

}

#endif // __TCPSTREAM_HPP__
