
#ifndef __TLSBUF_HPP__
#define __TLSBUF_HPP__


#include <cstdint>
#include "tcpstream.hpp"
#include "tls.hpp"
#include "networkstream.hpp"

namespace Zigurat
{

  class tlsbuf : public std::basic_streambuf<char>
  {
  protected:
    static const std::streamsize BUFFER_SIZE;

    tcpstream        _tcpstream;
    char_type       *_buffer;
    std::streamsize  _length;

    TLS::ProtocolVersion     _protocol_version = TLS::VERSION_1_2;
    TLS::HandshakeParameters _handshake_params;

    // OpenSSL owns the connection now. What used to sit here -- the read and
    // write cipher states, six key and IV pointers, two sequence numbers and a
    // running transcript of the handshake -- was the machinery of a TLS
    // implementation written in this repository, and none of it is wanted.
    //
    // _ssl owns the BIO wrapped round the socket. _tcpstream stays because it
    // opens and closes the socket and holds the timeouts; nothing reads or
    // writes through it any more, and doing so would take bytes out from under
    // the record layer.
    void* _ssl = nullptr;      // SSL*, cast inside tlsbuf.cpp

    // Who the peer turned out to be, once its certificate has been checked
    // against the authority, and what that certificate says it may do. The
    // permissions are the issuer's assertion travelling with the connection --
    // this end stores nothing about the peer and looks nothing up to learn them.
    std::string              _peer_subject;
    std::vector<std::string> _peer_permissions;

    // Fills _peer_subject and _peer_permissions from the certificate the peer
    // presented, once OpenSSL has decided it is one this authority vouched for.
    void _capture_peer();

  public:
    // Called from OpenSSL's verify callback once a certificate has been shown
    // to be genuine and issued by this end's authority. Takes the subject and
    // the permissions off it and asks the authorize hook whether this server
    // has heard of the holder. Public only because a C callback has to reach
    // it; nothing else should.
    bool authorize_peer(void*);

  protected:

  public:
    // The distinguished name on the peer's certificate. Empty until the
    // handshake has finished and the certificate has been accepted.
    const std::string& peer_subject() const;

    // What that certificate says its holder may do. Empty for a certificate
    // carrying no permissions extension, which means it may do nothing: an
    // issuer grants by naming, never by omitting.
    const std::vector<std::string>& peer_permissions() const;

  protected:

  public:    
    tlsbuf();
    tlsbuf(const tlsbuf&) = delete;
    tlsbuf(tlsbuf&&);

    tlsbuf& operator=(const tlsbuf&) = delete;
    tlsbuf& operator=(tlsbuf&&);

    virtual tlsbuf* open(TLS::ConnectionEnd, TLS::HandshakeParameters, Socket::handle_t, bool = true, int = 0);
    virtual tlsbuf* open(TLS::HandshakeParameters, std::string, std::string, bool = true, int = 0);

    virtual tlsbuf* setbuf(char_type*, std::streamsize) override;
    virtual pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;
    virtual pos_type seekpos(pos_type, std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;
    virtual int sync() override;
    virtual std::streamsize showmanyc() override;
    virtual int_type underflow() override;
    virtual int_type overflow(int_type = traits_type::eof()) override;
    virtual int_type pbackfail(int_type = traits_type::eof()) override;

    virtual bool is_open() const;
    virtual tlsbuf* close();
    virtual ~tlsbuf();
  };

}

#endif // __TLSBUF_HPP__
