#include "tlsbuf.hpp"
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>
#include <string>
#include "utility.hpp"
#include "tlsexception.hpp"
#include "x509.hpp"
#include "bufferstream.hpp"

#include <csignal>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/bio.h>


namespace Zigurat
{

  namespace
  {
    // What used to be here was a TLS 1.2 implementation written in this
    // repository: a record layer, a handshake, a key schedule, CBC padding and
    // a MAC comparison, about fifty kilobytes of it. It worked well enough to
    // talk to itself, which is very nearly the only thing it ever talked to.
    //
    // Three defects go with it rather than being fixed in place. The premaster
    // secret was decrypted with PKCS#1 v1.5 and a distinguishable error came
    // back when the padding was wrong, which is a Bleichenbacher oracle. The
    // CBC padding check read the length byte and never the padding behind it,
    // which is the other classic one. The MAC was compared with std::memcmp,
    // which stops at the first byte that differs and so takes longer over a
    // nearly-right forgery than a hopeless one.
    //
    // None of them are fixed here. They are gone because nothing in this file
    // implements TLS any more.

    // Writing to a socket whose peer has hung up raises SIGPIPE, and the
    // default disposition is to kill the process.
    //
    // socketbuf never had this problem: it sends with MSG_NOSIGNAL on Linux and
    // sets SO_NOSIGPIPE on the platforms that have it. OpenSSL's socket BIO
    // does neither -- it calls write() -- so handing it the descriptor brings
    // the hazard back, and it belongs to this library rather than to whoever
    // links it. ziguratip already ignores the signal in main; the test binary
    // does not, and it died here rather than reporting the case that exists to
    // catch exactly this.
    void ignore_sigpipe_once()
    {
#if !defined(_WIN32) && !defined(_WIN64)
      static std::once_flag once;
      std::call_once(once, [] () { std::signal(SIGPIPE, SIG_IGN); });
#endif
    }

    std::string ssl_error_text()
    {
      std::string text;
      unsigned long code;
      while ((code = ERR_get_error()) != 0) {
	char buffer[256];
	ERR_error_string_n(code, buffer, sizeof(buffer));
	if (!text.empty()) text += "; ";
	text += buffer;
      }
      return text.empty() ? std::string("no detail") : text;
    }

    // Where a tlsbuf finds itself again from inside OpenSSL's verify callback.
    int peer_index()
    {
      static const int index = SSL_get_ex_new_index(0, (void*)"tlsbuf", nullptr, nullptr, nullptr);
      return index;
    }

    // A certificate file, whichever way round it was written.
    // SSL_CTX_load_verify_locations reads PEM only, and ca writes DER unless
    // asked otherwise, so the authority is parsed here and put into the store
    // directly. That takes both without anyone having to know which they hold.
    void trust_authority(SSL_CTX* context, const std::string& path)
    {
      std::ifstream file(path, std::ios::binary);
      if (!file.good()) throw TLSException("cannot read the certificate authority: " + path);
      std::stringstream contents;
      contents << file.rdbuf();
      const std::string bytes = contents.str();
      if (bytes.empty()) throw TLSException("cannot read the certificate authority: it is empty");

      BIO* bio = BIO_new_mem_buf(bytes.data(), (int)bytes.size());
      if (bio == nullptr) throw TLSException("out of memory reading the certificate authority");

      ::X509* authority = d2i_X509_bio(bio, nullptr);
      if (authority == nullptr) {
	BIO_reset(bio);
	authority = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr);
      }
      BIO_free(bio);
      if (authority == nullptr)
	throw TLSException("cannot read the certificate authority: not a certificate");

      ::X509_STORE* store = SSL_CTX_get_cert_store(context);
      if (store == nullptr || X509_STORE_add_cert(store, authority) != 1) {
	::X509_free(authority);
	throw TLSException("cannot trust the certificate authority: " + ssl_error_text());
      }

      // Named as an acceptable issuer as well, so a client is told which
      // certificate to offer rather than having to guess.
      SSL_CTX_add_client_CA(context, authority);
      ::X509_free(authority);
    }

    int passphrase(char* buffer, int size, int, void* userdata)
    {
      const std::string* phrase = (const std::string*)userdata;
      if (phrase == nullptr) return 0;
      const size_t length = (phrase->size() < (size_t)size) ? phrase->size() : (size_t)size;
      std::memcpy(buffer, phrase->data(), length);
      return (int)length;
    }

    // OpenSSL decides whether a certificate is genuine and was issued by the
    // authority. Whether this server has heard of the holder is its own
    // question, and it is asked here, at the point the peer is named, so the
    // refusal is an alert in the handshake rather than a puzzle afterwards.
    int verify_peer(int preverified, ::X509_STORE_CTX* store)
    {
      if (preverified != 1) return 0;
      if (X509_STORE_CTX_get_error_depth(store) != 0) return 1;   // decide once, at the leaf

      SSL* ssl = (SSL*)X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx());
      if (ssl == nullptr) return 1;

      tlsbuf* buffer = (tlsbuf*)SSL_get_ex_data(ssl, peer_index());
      if (buffer == nullptr) return 1;

      if (buffer->authorize_peer(X509_STORE_CTX_get_current_cert(store))) return 1;

      X509_STORE_CTX_set_error(store, X509_V_ERR_CERT_REJECTED);
      return 0;
    }

    struct ContextKey
    {
      int entity;
      std::string certificate, private_key, cipher, authority;
      bool operator<(const ContextKey& other) const
      {
	if (entity != other.entity) return entity < other.entity;
	if (certificate != other.certificate) return certificate < other.certificate;
	if (private_key != other.private_key) return private_key < other.private_key;
	if (cipher != other.cipher) return cipher < other.cipher;
	return authority < other.authority;
      }
    };

    // One context per set of credentials, kept for the life of the process.
    //
    // The old code read the certificate, the key and the authority off the disk
    // on every handshake, several times each. A context holds the parsed
    // material and OpenSSL makes a connection from it, which is what it is for.
    SSL_CTX* context_for(TLS::ConnectionEnd entity, const TLS::Credentials& credentials)
    {
      static std::map<ContextKey, SSL_CTX*> contexts;
      static std::mutex guard;

      const ContextKey key { (int)entity, credentials.certificate, credentials.private_key,
			     credentials.private_key_cipher, credentials.authority };

      ignore_sigpipe_once();

      std::lock_guard<std::mutex> lock(guard);
      auto found = contexts.find(key);
      if (found != contexts.end()) return found->second;

      SSL_CTX* context = SSL_CTX_new((entity == TLS::ConnectionEnd::SERVER)
				     ? TLS_server_method() : TLS_client_method());
      if (context == nullptr) throw TLSException("cannot make a TLS context: " + ssl_error_text());

      try {
	// Nothing below TLS 1.2, and no record compression: CRIME.
	SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);

	// And nothing above it yet, which is a decision rather than an omission.
	//
	// This end refuses a peer by its subject, in the verify callback, and the
	// register is the only revocation the design has. Under TLS 1.3 the
	// client's certificate is not examined until after the client has sent
	// its Finished and considers the handshake done, so SSL_connect returns
	// success to a stranger and the refusal reaches it later, as an alert on
	// a connection it thinks it already has. The suite says an unregistered
	// subject is refused, and under 1.3 it was not -- it connected, and was
	// told afterwards.
	//
	// Allowing 1.3 means the client has to confirm the server accepted it
	// before reporting a connection, which is a change on the client side and
	// belongs with the rest of the modern-TLS work rather than here.
	SSL_CTX_set_max_proto_version(context, TLS1_2_VERSION);
	SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION | SSL_OP_CIPHER_SERVER_PREFERENCE);

	if (!credentials.certificate.empty()) {
	  // PEM or DER: the call wants to be told which, and ca writes either.
	  if (SSL_CTX_use_certificate_file(context, credentials.certificate.c_str(), SSL_FILETYPE_PEM) != 1 &&
	      SSL_CTX_use_certificate_file(context, credentials.certificate.c_str(), SSL_FILETYPE_ASN1) != 1)
	    throw TLSException("cannot read this end's certificate: " + credentials.certificate);
	}

	if (!credentials.private_key.empty()) {
	  SSL_CTX_set_default_passwd_cb(context, passphrase);
	  SSL_CTX_set_default_passwd_cb_userdata(context, (void*)&credentials.private_key_cipher);

	  if (SSL_CTX_use_PrivateKey_file(context, credentials.private_key.c_str(), SSL_FILETYPE_PEM) != 1 &&
	      SSL_CTX_use_PrivateKey_file(context, credentials.private_key.c_str(), SSL_FILETYPE_ASN1) != 1)
	    throw TLSException("cannot read this end's private key: " + credentials.private_key);

	  if (SSL_CTX_check_private_key(context) != 1)
	    throw TLSException("this end's private key does not belong to its certificate");
	}

	if (!credentials.authority.empty()) {
	  trust_authority(context, credentials.authority);

	  // Both ends present a certificate and both check it, which is what
	  // this arrangement has always done: a peer is who the authority says
	  // it is, or it does not get in.
	  SSL_CTX_set_verify(context, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verify_peer);
	  SSL_CTX_set_verify_depth(context, 1);   // one authority, signing leaves directly
	}
      } catch (...) {
	SSL_CTX_free(context);
	throw;
      }

      contexts[key] = context;
      return context;
    }
  }

  const std::streamsize tlsbuf::BUFFER_SIZE = 16384;

  tlsbuf::tlsbuf()
    : _tcpstream(), _buffer(nullptr), _length(0)
  {
    this->setg(nullptr, nullptr, nullptr);
    this->setp(nullptr, nullptr);
  }

  tlsbuf::tlsbuf(tlsbuf&& other)
    : _tcpstream(std::forward<tcpstream&&>(other._tcpstream)), _buffer(other._buffer), _length(other._length)
  {
    this->_handshake_params = other._handshake_params;
    this->_ssl = other._ssl;
    this->_peer_subject = std::move(other._peer_subject);
    this->_peer_permissions = std::move(other._peer_permissions);

    other._buffer = nullptr;
    other._length = 0;
    other._ssl = nullptr;

    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pptr(), other.epptr());

    // The verify callback finds its tlsbuf through the SSL, so a connection
    // that has moved has to be pointed at where it moved to.
    if (this->_ssl != nullptr) SSL_set_ex_data((SSL*)this->_ssl, peer_index(), this);
  }

  tlsbuf& tlsbuf::operator=(tlsbuf&& other)
  {
    if (this->_buffer != nullptr) delete[] this->_buffer;
    if (this->_ssl != nullptr) SSL_free((SSL*)this->_ssl);

    this->_tcpstream = std::forward<tcpstream&&>(other._tcpstream);
    this->_handshake_params = other._handshake_params;
    this->_buffer = other._buffer;
    this->_length = other._length;
    this->_ssl = other._ssl;
    this->_peer_subject = std::move(other._peer_subject);
    this->_peer_permissions = std::move(other._peer_permissions);

    other._buffer = nullptr;
    other._length = 0;
    other._ssl = nullptr;

    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pptr(), other.epptr());

    if (this->_ssl != nullptr) SSL_set_ex_data((SSL*)this->_ssl, peer_index(), this);

    return *this;
  }

  // Called from the verify callback with a certificate OpenSSL has already
  // decided is genuine and issued by the authority this end trusts. What is
  // left is who the holder is, and whether this server has heard of them.
  bool tlsbuf::authorize_peer(void* certificate)
  {
    ::X509* peer = (::X509*)certificate;
    if (peer == nullptr) return false;

    unsigned char* der = nullptr;
    const int length = i2d_X509(peer, &der);
    if (length <= 0 || der == nullptr) return false;

    bool allowed = true;
    try {
      bufferstream encoded;
      encoded.write((const char*)der, (std::streamsize)length);
      encoded.seekg(0, std::ios::beg);
      this->_peer_subject = X509::certificate_subject(encoded);

      encoded.clear();
      encoded.seekg(0, std::ios::beg);
      this->_peer_permissions = X509::certificate_permissions(encoded);

      if (this->_handshake_params.authorize)
	allowed = this->_handshake_params.authorize(this->_peer_subject);
    } catch (const std::exception&) {
      allowed = false;
    }
    OPENSSL_free(der);

    if (!allowed) {
      this->_peer_subject.clear();
      this->_peer_permissions.clear();
    }
    return allowed;
  }

  // The subject and the permissions were taken in the verify callback, where
  // the certificate was in hand. A peer that presented none leaves them empty,
  // which is what a connection with nobody on the other end of it should say.
  void tlsbuf::_capture_peer()
  {
    SSL* ssl = (SSL*)this->_ssl;
    if (ssl == nullptr) return;

    ::X509* peer = SSL_get1_peer_certificate(ssl);
    if (peer == nullptr) {
      this->_peer_subject.clear();
      this->_peer_permissions.clear();
      return;
    }
    ::X509_free(peer);
  }

  tlsbuf* tlsbuf::open(TLS::ConnectionEnd entity, TLS::HandshakeParameters params,
		       Socket::handle_t handle, bool blocking_mode, int timeout)
  {
    this->_handshake_params = params;

    if (this->_buffer == nullptr)
      this->setbuf(new char_type[tlsbuf::BUFFER_SIZE], tlsbuf::BUFFER_SIZE);

    // The socket keeps its owner and its timeouts here; the bytes on it belong
    // to OpenSSL from now on, and nothing may read or write through _tcpstream
    // again -- its read-ahead would take them out from under the record layer.
    this->_tcpstream.open(handle, blocking_mode, timeout);

    SSL_CTX* context = context_for(entity, this->_handshake_params.credentials);
    SSL* ssl = SSL_new(context);
    if (ssl == nullptr) throw TLSException("cannot make a TLS connection: " + ssl_error_text());
    this->_ssl = ssl;
    SSL_set_ex_data(ssl, peer_index(), this);

    BIO* bio = BIO_new_socket((int)handle, BIO_NOCLOSE);
    if (bio == nullptr) throw TLSException("cannot wrap the socket: " + ssl_error_text());
    SSL_set_bio(ssl, bio, bio);

    const int done = (entity == TLS::ConnectionEnd::SERVER) ? SSL_accept(ssl) : SSL_connect(ssl);
    if (done != 1)
      throw TLSException(std::string((entity == TLS::ConnectionEnd::SERVER)
				     ? "the peer was refused: " : "this end was refused: ") + ssl_error_text());

    this->_capture_peer();
    return this;
  }

  tlsbuf* tlsbuf::open(TLS::HandshakeParameters params, std::string node, std::string service,
		       bool blocking_mode, int timeout)
  {
    this->_handshake_params = params;

    if (this->_buffer == nullptr)
      this->setbuf(new char_type[tlsbuf::BUFFER_SIZE], tlsbuf::BUFFER_SIZE);

    this->_tcpstream.open(node, service, blocking_mode, timeout);

    SSL_CTX* context = context_for(TLS::ConnectionEnd::CLIENT, this->_handshake_params.credentials);
    SSL* ssl = SSL_new(context);
    if (ssl == nullptr) throw TLSException("cannot make a TLS connection: " + ssl_error_text());
    this->_ssl = ssl;
    SSL_set_ex_data(ssl, peer_index(), this);

    // Which host was asked for, so a server holding more than one certificate
    // can answer with the right one.
    if (!node.empty()) SSL_set_tlsext_host_name(ssl, node.c_str());

    BIO* bio = BIO_new_socket((int)this->_tcpstream.handle(), BIO_NOCLOSE);
    if (bio == nullptr) throw TLSException("cannot wrap the socket: " + ssl_error_text());
    SSL_set_bio(ssl, bio, bio);

    if (SSL_connect(ssl) != 1)
      throw TLSException("this end was refused: " + ssl_error_text());

    this->_capture_peer();
    return this;
  }

  const std::string& tlsbuf::peer_subject() const
  {
    return this->_peer_subject;
  }

  const std::vector<std::string>& tlsbuf::peer_permissions() const
  {
    return this->_peer_permissions;
  }

  tlsbuf* tlsbuf::setbuf(char_type* s, std::streamsize n)
  {
    if (this->_buffer != nullptr) delete[] this->_buffer;

    this->_buffer = s;
    this->_length = n;

    this->setg(this->_buffer, this->_buffer + this->_length, this->_buffer + this->_length);
    this->setp(this->_buffer, this->_buffer + this->_length);

    return this;
  }

  tlsbuf::pos_type tlsbuf::seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode)
  {
    return pos_type(off_type(-1));
  }

  tlsbuf::pos_type tlsbuf::seekpos(pos_type, std::ios_base::openmode)
  {
    return pos_type(off_type(-1));
  }

  // Flushing pushes what has been written out. It used to pull as well, which
  // made every flush block until the peer happened to say something back.
  int tlsbuf::sync()
  {
    if (this->overflow(traits_type::eof()) == traits_type::eof()) return -1;
    return 0;
  }

  std::streamsize tlsbuf::showmanyc()
  {
    return 0;
  }

  tlsbuf::int_type tlsbuf::underflow()
  {
    if (this->gptr() != nullptr && this->gptr() < this->egptr())
      return traits_type::to_int_type(*this->gptr());

    // About to block waiting for the peer, so anything still queued for it has
    // to go out first -- the same rule socketbuf follows, and for the same
    // reason. Without it a request and its reply deadlock, the reply sitting
    // unsent in the put area while both ends wait to read.
    if (this->pptr() != nullptr && this->pptr() > this->pbase())
      this->overflow(traits_type::eof());

    SSL* ssl = (SSL*)this->_ssl;
    if (ssl == nullptr) return traits_type::eof();

    const int taken = SSL_read(ssl, this->_buffer, (int)this->_length);
    if (taken <= 0) {
      // A peer that shut down cleanly and a peer that vanished say the same
      // thing to a reader: there is nothing more coming.
      this->setg(nullptr, nullptr, nullptr);
      return traits_type::eof();
    }

    this->setg(this->_buffer, this->_buffer, this->_buffer + taken);
    return traits_type::to_int_type(*this->gptr());
  }

  tlsbuf::int_type tlsbuf::overflow(int_type ch)
  {
    SSL* ssl = (SSL*)this->_ssl;
    if (ssl == nullptr) return traits_type::eof();

    const std::streamsize pending = (this->pptr() != nullptr) ? (this->pptr() - this->pbase()) : 0;

    std::streamsize written = 0;
    while (written < pending) {
      const int sent = SSL_write(ssl, this->pbase() + written, (int)(pending - written));
      if (sent <= 0) {
	this->setp(nullptr, nullptr);
	return traits_type::eof();
      }
      written += sent;
    }

    this->setp(this->_buffer, this->_buffer + this->_length);

    if (!traits_type::eq_int_type(ch, traits_type::eof())) {
      *this->pptr() = traits_type::to_char_type(ch);
      this->pbump(1);
    }

    return traits_type::not_eof(ch);
  }

  tlsbuf::int_type tlsbuf::pbackfail(int_type)
  {
    return traits_type::eof();
  }

  bool tlsbuf::is_open() const
  {
    return this->_tcpstream.is_open();
  }

  tlsbuf* tlsbuf::close()
  {
    if (this->_ssl != nullptr) {
      SSL* ssl = (SSL*)this->_ssl;

      // close_notify, and only the sending half of it: waiting for the peer's
      // in reply is what a second SSL_shutdown does, and a peer that has gone
      // already is not an error, there is nobody left to tell.
      SSL_shutdown(ssl);

      SSL_free(ssl);          // takes the BIO with it
      this->_ssl = nullptr;
    }

    this->_tcpstream.close();
    return this;
  }

  tlsbuf::~tlsbuf()
  {
    if (this->_ssl != nullptr) {
      SSL_free((SSL*)this->_ssl);
      this->_ssl = nullptr;
    }

    this->_tcpstream.close();
    delete[] this->_buffer;
  }

}
