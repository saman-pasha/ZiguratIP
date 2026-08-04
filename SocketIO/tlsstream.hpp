
#ifndef __TLSSTREAM_HPP__
#define __TLSSTREAM_HPP__

#include "nbostream.hpp"
#include "tlsbuf.hpp"

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

    // Who the other end proved itself to be, and what its certificate says it
    // may do. Both are settled during the handshake, so both are ready before
    // anything is read or written.
    const std::string& peer_subject() const;
    const std::vector<std::string>& peer_permissions() const;

    void swap(tlsstream&);

    virtual ~tlsstream();
  };

}

#endif // __TLSSTREAM_HPP__
