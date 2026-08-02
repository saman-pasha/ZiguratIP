
#ifndef __TLSSTREAM_H__
#define __TLSSTREAM_H__

#include "nbostream.h"
#include "tlsbuf.h"

namespace Zigurat
{

  class tlsstream : public nbostream
  {
  public:
    using nbostream::nbostream;
    
    tlsstream();
    explicit tlsstream(TLS::ConnectionEnd, TLS::HandshakeParameters, Socket::handle_t, bool = true, int = 0);
    explicit tlsstream(TLS::HandshakeParameters, std::string, std::string, bool = true, int = 0);

    tlsstream& operator=(const tlsstream&) = delete;
    tlsstream& operator=(tlsstream&&);

    void open(TLS::ConnectionEnd, TLS::HandshakeParameters, Socket::handle_t, bool = true, int = 0);
    void open(TLS::HandshakeParameters, std::string, std::string, bool = true, int = 0);
    bool is_open() const;
    void close();

    void swap(tlsstream&);

    virtual ~tlsstream();
  };

}

#endif // __TLSSTREAM_H__
