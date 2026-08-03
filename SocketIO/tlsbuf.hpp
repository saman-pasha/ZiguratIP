
#ifndef __TLSBUF_HPP__
#define __TLSBUF_HPP__

#include "tcpstream.hpp"
#include "tls.hpp"
#include "bufferstream.hpp"

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

    // One counter per direction. A single shared counter was incremented by
    // both the sender and the receiver, so the two ends disagreed about the
    // sequence number the moment traffic stopped being strictly alternating,
    // and every MAC after that point failed. Each is reset when the cipher
    // state changes, which is what a ChangeCipherSpec means.
    uint64_t                 _read_sequence_number = 0;
    uint64_t                 _write_sequence_number = 0;

    // TLS keeps one cipher state per direction, and they change at different
    // moments: a ChangeCipherSpec only ever settles the direction it travels in.
    // A single shared state meant that sending one also switched this end's
    // reading, so anything the peer sent before its own ChangeCipherSpec -- an
    // alert refusing the handshake, most importantly -- was decrypted with keys
    // the peer had not started using, and came out as noise.
    TLS::SecurityParameters  _read_state, _write_state, _pending_state;

    uint8_t *client_write_MAC_key = nullptr;
    uint8_t *server_write_MAC_key = nullptr;
    uint8_t *client_write_key     = nullptr;
    uint8_t *server_write_key     = nullptr;
    uint8_t *client_write_IV      = nullptr;
    uint8_t *server_write_IV      = nullptr;

    // Every handshake message that crosses the connection, in order and exactly
    // as it appeared, header included. Finished proves both ends saw the same
    // conversation, and CertificateVerify signs it, so it has to be the bytes
    // rather than a re-encoding of them.
    bufferstream _transcript;

    void _transcribe(binarystream&);
    void _transcript_hash(uint8_t*);
    void _transcript_bytes(binarystream&);

    void _send_record(TLS::Record&);
    void _recv_record(TLS::Record&);
    void _alert(TLS::AlertLevel, TLS::AlertDescription);
    void _send_handshake(TLS::Handshake&);
    void _recv_handshake(TLS::Handshake&);

    // Who the peer turned out to be, once its certificate has been checked
    // against the authority. Nothing is keyed on it yet; it is what a permission
    // would be decided from.
    std::string _peer_subject;

    void _credential(const std::string&, binarystream&);
    void _check_peer_certificate(binarystream&);

    void _send_certificate();
    void _recv_certificate(binarystream&);
    void _send_change_cipher_spec();
    void _recv_change_cipher_spec();
    void _derive_keys(binarystream&);
    void _send_finished(const char*);
    void _recv_finished(const char*);

    void _server_hello();
    void _server_handshake();
    void _client_hello();
    void _client_handshake();

  public:
    // The distinguished name on the peer's certificate. Empty until the
    // handshake has finished and the certificate has been accepted.
    const std::string& peer_subject() const;

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

    virtual void renegotiate();    
    virtual bool is_open() const;
    virtual tlsbuf* close();
    virtual ~tlsbuf();
  };

}

#endif // __TLSBUF_HPP__
