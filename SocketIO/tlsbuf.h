
#ifndef __TLSBUF_H__
#define __TLSBUF_H__

#include "tcpstream.h"
#include "tls.h"

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
    uint64_t                 _sequence_number = 0;
    TLS::SecurityParameters  _current_state, _pending_state;

    uint8_t *client_write_MAC_key;
    uint8_t *server_write_MAC_key;
    uint8_t *client_write_key;
    uint8_t *server_write_key;
    uint8_t *client_write_IV;
    uint8_t *server_write_IV;

    void _send_record(TLS::Record&);
    void _recv_record(TLS::Record&);
    void _alert(TLS::AlertLevel, TLS::AlertDescription);
    void _send_handshake(TLS::Handshake&);
    void _recv_handshake(TLS::Handshake&);

    void _server_hello();
    void _server_handshake();
    void _client_hello();
    void _client_handshake();

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

    virtual void renegotiate();    
    virtual bool is_open() const;
    virtual tlsbuf* close();
    virtual ~tlsbuf();
  };

}

#endif // __TLSBUF_H__
