#include "tlsbuf.hpp"
#include <cstring>
#include "utility.hpp"
#include "bufferstream.hpp"
#include "arraystream.hpp"
#include "tlsexception.hpp"
#include "zlibhelper.hpp"
#include "aes.hpp"
#include "shahelper.hpp"
#include "x509.hpp"
#include "der.hpp"
#include "filestream.hpp"
#include <vector>
#include <string>
#include <cassert>
#include <cstdlib>


namespace Zigurat
{

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
    other._buffer = nullptr;
    other._length = 0;

    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pptr(), other.epptr());
  }

  tlsbuf& tlsbuf::operator=(tlsbuf&& other)
  {
    if (this->_buffer != nullptr) delete[] this->_buffer;

    this->_tcpstream = std::forward<tcpstream&&>(other._tcpstream);
    this->_buffer = other._buffer;
    this->_length = other._length;

    other._buffer = nullptr;
    other._length = 0;

    this->setg(other.eback(), other.gptr(), other.egptr());
    this->setp(other.pptr(), other.epptr());

    return *this;
  }

  void tlsbuf::_transcribe(binarystream& message)
  {
    const std::streamsize length = message.length();
    message.read(this->_transcript, 0, length);
  }

  // SHA-256 over everything said so far. TLS 1.2 ties the digest to the cipher
  // suite's PRF hash, and every suite here uses SHA-256.
  void tlsbuf::_transcript_hash(uint8_t* digest)
  {
    const std::streamsize length = this->_transcript.length();
    bufferstream copy;
    this->_transcript.read(copy, 0, length);

    uint8_t* bytes = new uint8_t[(size_t)(length > 0 ? length : 1)];
    copy.read((char*)bytes, length);
    SHA::checksum(SHA::SHA256, bytes, (size_t)length, digest);
    delete[] bytes;
  }

  void tlsbuf::_send_record(TLS::Record &record)
  {
    if (record.length > tlsbuf::BUFFER_SIZE) throw TLSException("invalid fragment length");
    
    TLS::SecurityParameters &params = this->_current_state;
    uint64_t        sequence_number = this->_write_sequence_number++;
      
    this->_tcpstream.write_std_ubyte((uint8_t)record.type);
    this->_tcpstream.write_std_ubyte(record.version.major);
    this->_tcpstream.write_std_ubyte(record.version.minor);

    bufferstream compressed;
    switch (params.compression_algorithm) {
    case TLS::CompressionMethod::NONE:
      record.fragment.read(compressed, record.length);
      break;
    case TLS::CompressionMethod::DEFLATE:
      ZLib::compress(ZLib::DEFLATE, record.fragment, record.length, compressed);
      break;
    default:
      throw TLSException("unsupported compression algorithm");
    }
    uint16_t compressed_length = compressed.tellp();
    
    if (params.bulk_cipher_algorithm == TLS::BulkCipherAlgorithm::NONE) {

      if (params.mac_algorithm == TLS::MACAlgorithm::NONE) {
	this->_tcpstream.write_std_ushort(compressed_length);
	compressed.read(this->_tcpstream, compressed_length);
      } else {
	this->_tcpstream.write_std_ushort(compressed_length + params.mac_length);
	compressed.read(this->_tcpstream, compressed_length);
	compressed.seekg(0, std::ios_base::beg);
	uint8_t mac[params.mac_length];
	TLS::MAC(params.mac_algorithm,
		 (params.entity == TLS::ConnectionEnd::SERVER) ? this->server_write_MAC_key : this->client_write_MAC_key,
		 params.mac_key_length, sequence_number, record.type, record.version, compressed, compressed_length, mac);
	this->_tcpstream.write((char*)mac, params.mac_length);
      }

    } else if (params.bulk_cipher_algorithm == TLS::BulkCipherAlgorithm::AES) {

      if (params.cipher_type == TLS::CipherType::BLOCK) {

	// plain = compressed || MAC || padding, padded so the whole of it is a
	// whole number of blocks, then chained under a fresh IV that goes out in
	// front of it in the clear. The IV is not enciphered and is not part of
	// what the MAC covers.
	bufferstream plain;

	compressed.read(plain, compressed_length);

	uint8_t mac[params.mac_length];
	compressed.seekg(0, std::ios_base::beg);
	TLS::MAC(params.mac_algorithm,
		 (params.entity == TLS::ConnectionEnd::SERVER) ? this->server_write_MAC_key : this->client_write_MAC_key,
		 params.mac_key_length, sequence_number, record.type, record.version, compressed, compressed_length, mac);
	plain.write((char*)mac, params.mac_length);

	std::streamsize unpadded = compressed_length + params.mac_length + sizeof(uint8_t);
	uint8_t padding_length = (uint8_t)((params.block_length - (unpadded % params.block_length)) % params.block_length);
	plain.fill_n(padding_length + 1, padding_length);

	uint8_t iv[params.record_iv_length];
	TLS::IV(iv, params.record_iv_length);

	const std::streamsize plain_length = plain.length();
	this->_tcpstream.write_std_ushort((uint16_t)(params.record_iv_length + plain_length));
	this->_tcpstream.write((char*)iv, params.record_iv_length);

	if (params.enc_key_length == 16) {

	  AES128::key_t write_key;
	  std::memcpy(write_key, (params.entity == TLS::ConnectionEnd::SERVER) ? this->server_write_key : this->client_write_key, params.enc_key_length);
	  AES128::schedule_t expanded_key;
	  AES128::KeyExpansion(write_key, expanded_key);
	  AES128::CipherCBC(plain, expanded_key, iv, this->_tcpstream);

	} else if (params.enc_key_length == 32) {

	  AES256::key_t write_key;
	  std::memcpy(write_key, (params.entity == TLS::ConnectionEnd::SERVER) ? this->server_write_key : this->client_write_key, params.enc_key_length);
	  AES256::schedule_t expanded_key;
	  AES256::KeyExpansion(write_key, expanded_key);
	  AES256::CipherCBC(plain, expanded_key, iv, this->_tcpstream);

	} else {
	  throw TLSException("unsupported encryption key length");
	}
      } else {
	throw TLSException("unsupported cipher type");
      }
    } else {
      throw TLSException("unsupported bulk cipher algorithm");
    }

    this->_tcpstream.flush();
  }
  
  void tlsbuf::_recv_record(TLS::Record &record)
  {
    TLS::SecurityParameters &params = this->_current_state;
    uint64_t        sequence_number;
    
    do {

      const std::streampos start = record.fragment.tellg();
      sequence_number = this->_read_sequence_number++;
      
      record.type = (TLS::ContentType)this->_tcpstream.read_std_ubyte();
      record.version = {this->_tcpstream.read_std_ubyte(), this->_tcpstream.read_std_ubyte()};

      uint16_t compressed_length;
      bufferstream compressed;
      if (params.bulk_cipher_algorithm == TLS::BulkCipherAlgorithm::NONE) {

	if (params.mac_algorithm == TLS::MACAlgorithm::NONE) {
	  compressed_length = this->_tcpstream.read_std_ushort();
	  this->_tcpstream.read(compressed, compressed_length);
	} else {
	  compressed_length = this->_tcpstream.read_std_ushort() - params.mac_length;
	  this->_tcpstream.read(compressed, compressed_length);
	
	  uint8_t mac[params.mac_length];
	  TLS::MAC(params.mac_algorithm,
		   (params.entity == TLS::ConnectionEnd::SERVER) ? this->client_write_MAC_key : this->server_write_MAC_key,
		   params.mac_key_length, sequence_number, record.type, record.version, compressed, compressed_length, mac);
	  compressed.seekg(0, std::ios_base::beg);
	
	  uint8_t record_mac[params.mac_length];
	  this->_tcpstream.read((char*)record_mac, params.mac_length);

	  if (std::memcmp(mac, record_mac, params.mac_length) != 0)	  
	    this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::BAD_RECORD_MAC);
	}

      } else if (params.bulk_cipher_algorithm == TLS::BulkCipherAlgorithm::AES) {

	if (params.cipher_type == TLS::CipherType::BLOCK) {

	  // The IV came in front, in the clear, and is not part of the cipher text.
	  uint16_t record_length = this->_tcpstream.read_std_ushort();
	  if (record_length < params.record_iv_length + params.block_length)
	    this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::DECODE_ERROR);

	  uint8_t iv[params.record_iv_length];
	  this->_tcpstream.read_exact((char*)iv, params.record_iv_length);

	  const uint16_t cipher_text_length = record_length - params.record_iv_length;
	  bufferstream cipher_text;
	  this->_tcpstream.read_exact(cipher_text, cipher_text_length);

	  bufferstream input;
	  if (params.enc_key_length == 16) {

	    AES128::key_t write_key;
	    std::memcpy(write_key, (params.entity == TLS::ConnectionEnd::SERVER) ? this->client_write_key : this->server_write_key, params.enc_key_length);
	    AES128::schedule_t expanded_key;
	    AES128::KeyExpansion(write_key, expanded_key);
	    AES128::InverseCipherCBC(cipher_text, expanded_key, iv, input);

	  } else if (params.enc_key_length == 32) {

	    AES256::key_t write_key;
	    std::memcpy(write_key, (params.entity == TLS::ConnectionEnd::SERVER) ? this->client_write_key : this->server_write_key, params.enc_key_length);
	    AES256::schedule_t expanded_key;
	    AES256::KeyExpansion(write_key, expanded_key);
	    AES256::InverseCipherCBC(cipher_text, expanded_key, iv, input);

	  } else {
	    throw TLSException("unsupported encryption key length");
	  }

	  const uint8_t padding_length = (uint8_t)input.at(cipher_text_length - 1);
	  if (cipher_text_length < params.mac_length + padding_length + 1)
	    this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::BAD_RECORD_MAC);

	  compressed_length = cipher_text_length - params.mac_length - padding_length - 1;
	  input.read(compressed, compressed_length);

	  uint8_t mac[params.mac_length];
	  TLS::MAC(params.mac_algorithm,
		   (params.entity == TLS::ConnectionEnd::SERVER) ? this->client_write_MAC_key : this->server_write_MAC_key,
		   params.mac_key_length, sequence_number, record.type, record.version, compressed, compressed_length, mac);
	  compressed.seekg(0, std::ios_base::beg);

	  uint8_t record_mac[params.mac_length];
	  input.read((char*)record_mac, params.mac_length);

	  if (std::memcmp(mac, record_mac, params.mac_length) != 0)
	    this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::BAD_RECORD_MAC);
	} else {
	  throw TLSException("unsupported cipher type");
	}
      } else {
	throw TLSException("unsupported bulk cipher algorithm");
      }

      switch (params.compression_algorithm) {
      case TLS::CompressionMethod::NONE:
	compressed.read(record.fragment, compressed_length);
	record.length = compressed_length;
	break;
      case TLS::CompressionMethod::DEFLATE:
	ZLib::decompress(ZLib::DEFLATE, compressed, compressed_length, record.fragment);
	record.length = record.fragment.tellp();
	break;
      default:
	throw TLSException("unsupported compression algorithm");
      }

      // Where this record's fragment began, so anything read to inspect it can be
      // put back. These used to rewind by -record.length from wherever the read
      // had left the cursor, which for a one octet peek is a seek to a negative
      // position: it fails, the stream goes bad, and every subsequent read
      // returns nothing. On the client that meant every handshake message
      // arrived empty and was rejected as unexpected.
      const std::streampos fragment_begin = start;

      if (record.type == TLS::ContentType::ALERT) {
	TLS::AlertLevel level = (TLS::AlertLevel)record.fragment.read_std_ubyte();
	TLS::AlertDescription description = (TLS::AlertDescription)record.fragment.read_std_ubyte();
	if (level == TLS::AlertLevel::FATAL)
	  throw TLSException("fatal alert " + std::to_string((uint8_t)description));

	// A warning -- close_notify among them -- is noted and the wait resumes.
	record.type = (TLS::ContentType)0;
	record.version = {0, 0};
	record.length = 0;
	record.fragment.clear();
	record.fragment.seekg(fragment_begin, std::ios_base::beg);
	record.fragment.seekp(fragment_begin, std::ios_base::beg);
	continue;

      } else if (record.type == TLS::ContentType::HANDSHAKE && params.entity == TLS::ConnectionEnd::CLIENT) {

	TLS::HandshakeType msg_type = (TLS::HandshakeType)record.fragment.read_std_ubyte();
	record.fragment.clear();
	record.fragment.seekg(fragment_begin, std::ios_base::beg);

	if (msg_type == TLS::HandshakeType::HELLO_REQUEST) {
	  record.type = (TLS::ContentType)0;
	  record.version = {0, 0};
	  record.length = 0;
	  record.fragment.seekp(fragment_begin, std::ios_base::beg);
	  this->_client_handshake();
	  continue;
	}

	break;

      } else {
	break;
      }
      
    } while(true);
  }

  void tlsbuf::_alert(TLS::AlertLevel level, TLS::AlertDescription description)
  {
    bufferstream plain_text;
    plain_text.write_std_ubyte((uint8_t)level);
    plain_text.write_std_ubyte((uint8_t)description);
    
    TLS::Record record {TLS::ContentType::ALERT, this->_protocol_version, plain_text.tellp(), plain_text};
    
    this->_send_record(record);

    if (level == TLS::AlertLevel::FATAL)
      throw TLSException("fatal alert " + std::to_string((uint8_t)description));
  }

  // The four octet header -- one of type, three of length -- is part of what
  // goes on the wire, so the fragmenting below counts it. Measuring against
  // handshake.length alone left the last four octets of every message unsent.
  void tlsbuf::_send_handshake(TLS::Handshake& handshake)
  {
    bufferstream plain_text;
    plain_text.write_std_ubyte((uint8_t)handshake.msg_type);
    TLS::uint24(handshake.length, plain_text);
    handshake.body.read(plain_text, 0, handshake.length);

    this->_transcribe(plain_text);

    const std::streamsize total = plain_text.length();
    std::streamsize count = 0;
    TLS::Record     record {TLS::ContentType::HANDSHAKE, this->_protocol_version, 0, plain_text};
    do {
      record.length = std::min(total - count, this->BUFFER_SIZE);
      count        += record.length;
      this->_send_record(record);
    } while (count < total);
  }

  void tlsbuf::_recv_handshake(TLS::Handshake& handshake)
  {
    bufferstream plain_text;
    TLS::Record record {(TLS::ContentType)0, {0, 0}, 0, plain_text};
    this->_recv_record(record);

    // An assert would be compiled out of a release build, and "the peer sent
    // something other than a handshake message" is exactly the case that must
    // still be refused there.
    if (record.type != TLS::ContentType::HANDSHAKE)
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);

    handshake.msg_type = (TLS::HandshakeType)plain_text.read_std_ubyte();
    handshake.length   = TLS::uint24(plain_text);

    // What the first record carried beyond the four octet header. This was
    // adding the header rather than subtracting it, so the count ran two octets
    // over the body on every message and a fragmented one was never reassembled.
    std::streamsize count = record.length - (sizeof(uint8_t) * 4);
    plain_text.read(handshake.body, count);

    while (count < handshake.length) {
      this->_recv_record(record);
      if (record.type != TLS::ContentType::HANDSHAKE)
	this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);
      plain_text.read(handshake.body, record.length);
      count += record.length;
    }

    // The transcript is the messages as they appeared on the wire, header and
    // all, because that is what Finished and CertificateVerify sign over.
    bufferstream transcribed;
    transcribed.write_std_ubyte((uint8_t)handshake.msg_type);
    TLS::uint24(handshake.length, transcribed);
    handshake.body.read(transcribed, 0, handshake.length);
    this->_transcribe(transcribed);
  }

  const std::string& tlsbuf::peer_subject() const
  {
    return this->_peer_subject;
  }

  // A credential named in the parameters, read off disk into memory. Each is
  // opened fresh every time it is needed: X509 consumes the stream it is given,
  // and the handshake needs some of them more than once.
  void tlsbuf::_credential(const std::string& path, binarystream& content)
  {
    if (path.empty()) throw TLSException("no certificate configured for this end");

    filestream file(path, std::ios::in | std::ios::binary);
    if (!file.good()) throw TLSException("cannot read '" + path + "'");

    file.read(content, file.length());
  }

  // The whole of the trust decision. There is no chain to walk: either the
  // owner's authority signed this certificate or the peer has no business here.
  void tlsbuf::_check_peer_certificate(binarystream& certificate)
  {
    bufferstream authority, puk_info, authority_key;
    this->_credential(this->_handshake_params.credentials.authority, authority);

    try {
      X509::certificate_public_key(authority, puk_info);
      DER::encode_sequence(authority_key, puk_info);
    } catch (const std::exception& error) {
      throw TLSException(std::string("cannot read the certificate authority: ") + error.what());
    }

    bufferstream to_validate;
    certificate.read(to_validate, 0, certificate.length());

    try {
      X509::validate_by_puk(authority_key, to_validate);
    } catch (const std::exception&) {
      // Whatever the reason -- a signature that does not check out, a shape that
      // does not parse -- the peer is not one this authority vouched for.
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNKNOWN_CA);
    }

    bufferstream for_subject;
    certificate.read(for_subject, 0, certificate.length());
    this->_peer_subject = X509::certificate_subject(for_subject);
  }

  void tlsbuf::_send_certificate()
  {
    bufferstream certificate, body;
    this->_credential(this->_handshake_params.credentials.certificate, certificate);

    // certificate_list: one entry, since the peer already holds the authority.
    const std::streamsize length = certificate.length();
    TLS::uint24(length + 3, body);
    TLS::uint24(length, body);
    certificate.read(body, 0, length);

    TLS::Handshake message {TLS::HandshakeType::CERTIFICATE, body.length(), body};
    this->_send_handshake(message);
  }

  void tlsbuf::_recv_certificate(binarystream& certificate)
  {
    bufferstream body;
    TLS::Handshake message {(TLS::HandshakeType)0, 0, body};
    this->_recv_handshake(message);

    if (message.msg_type != TLS::HandshakeType::CERTIFICATE)
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);

    const uint32_t list_length = TLS::uint24(body);
    if (list_length == 0)
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::BAD_CERTIFICATE);

    const uint32_t first_length = TLS::uint24(body);
    body.read(certificate, first_length);
  }

  void tlsbuf::_send_change_cipher_spec()
  {
    bufferstream body;
    body.write_std_ubyte((uint8_t)TLS::CipherSpecType::CHANGE_CIPHER_SPEC);

    TLS::Record record {TLS::ContentType::CHANGE_CIPHER_SPEC, this->_protocol_version, 1, body};
    this->_send_record(record);

    // Everything after this goes out under the new state, counted from zero.
    this->_current_state = this->_pending_state;
    this->_write_sequence_number = 0;
  }

  void tlsbuf::_recv_change_cipher_spec()
  {
    bufferstream body;
    TLS::Record record {(TLS::ContentType)0, {0, 0}, 0, body};
    this->_recv_record(record);

    if (record.type != TLS::ContentType::CHANGE_CIPHER_SPEC)
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);

    this->_current_state = this->_pending_state;
    this->_read_sequence_number = 0;
  }

  // master_secret = PRF(pre_master_secret, "master secret",
  //                     client_random + server_random)[0..47], and the write
  //  keys from it. Both ends run this over the same inputs and never exchange
  //  the result.
  void tlsbuf::_derive_keys(binarystream& pre_master_secret)
  {
    TLS::SecurityParameters& params = this->_pending_state;

    uint8_t randoms[TLS::RANDOM_LENGTH * 2];
    std::memcpy(randoms, params.client_random, TLS::RANDOM_LENGTH);
    std::memcpy(randoms + TLS::RANDOM_LENGTH, params.server_random, TLS::RANDOM_LENGTH);

    const std::streamsize length = pre_master_secret.length();
    std::vector<uint8_t> secret((size_t)(length > 0 ? length : 1));
    pre_master_secret.read((char*)secret.data(), 0, length);

    TLS::PRF(params.prf_algorithm,
	     secret.data(),             (size_t)length,
	     (const uint8_t*)"master secret", 13,
	     randoms,                   sizeof(randoms),
	     params.master_secret,      TLS::MASTER_SECRET_LENGTH);

    delete[] this->client_write_MAC_key;
    delete[] this->server_write_MAC_key;
    delete[] this->client_write_key;
    delete[] this->server_write_key;
    delete[] this->client_write_IV;
    delete[] this->server_write_IV;

    this->client_write_MAC_key = new uint8_t[params.mac_key_length  ? params.mac_key_length  : 1];
    this->server_write_MAC_key = new uint8_t[params.mac_key_length  ? params.mac_key_length  : 1];
    this->client_write_key     = new uint8_t[params.enc_key_length  ? params.enc_key_length  : 1];
    this->server_write_key     = new uint8_t[params.enc_key_length  ? params.enc_key_length  : 1];
    this->client_write_IV      = new uint8_t[params.fixed_iv_length ? params.fixed_iv_length : 1];
    this->server_write_IV      = new uint8_t[params.fixed_iv_length ? params.fixed_iv_length : 1];

    TLS::calculate_keys(params,
			this->client_write_MAC_key, this->server_write_MAC_key,
			this->client_write_key,     this->server_write_key,
			this->client_write_IV,      this->server_write_IV);
  }

  // verify_data = PRF(master_secret, label, hash of every handshake message so
  // far)[0..11]. It proves both ends saw the same conversation, so a tampered
  // hello cannot survive to here.
  void tlsbuf::_send_finished(const char* label)
  {
    uint8_t digest[SHA::size(SHA::SHA256)];
    this->_transcript_hash(digest);          // before this message joins it

    bufferstream body;
    uint8_t verify_data[12];
    TLS::PRF(this->_current_state.prf_algorithm,
	     this->_current_state.master_secret, TLS::MASTER_SECRET_LENGTH,
	     (const uint8_t*)label, std::strlen(label),
	     digest, sizeof(digest),
	     verify_data, sizeof(verify_data));

    body.write((char*)verify_data, sizeof(verify_data));

    TLS::Handshake message {TLS::HandshakeType::FINISHED, body.length(), body};
    this->_send_handshake(message);
  }

  void tlsbuf::_recv_finished(const char* label)
  {
    uint8_t digest[SHA::size(SHA::SHA256)];
    this->_transcript_hash(digest);          // before the incoming message joins it

    bufferstream body;
    TLS::Handshake message {(TLS::HandshakeType)0, 0, body};
    this->_recv_handshake(message);

    if (message.msg_type != TLS::HandshakeType::FINISHED)
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);

    uint8_t expected[12];
    TLS::PRF(this->_current_state.prf_algorithm,
	     this->_current_state.master_secret, TLS::MASTER_SECRET_LENGTH,
	     (const uint8_t*)label, std::strlen(label),
	     digest, sizeof(digest),
	     expected, sizeof(expected));

    uint8_t received[12];
    if (message.length != (std::streamsize)sizeof(received))
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::DECODE_ERROR);
    body.read((char*)received, 0, (std::streamsize)sizeof(received));

    if (std::memcmp(expected, received, sizeof(expected)) != 0)
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::DECRYPT_ERROR);
  }

  void tlsbuf::_server_hello()
  {
    bufferstream client_body, server_body;
    TLS::Handshake client_hello {(TLS::HandshakeType)0, 0, client_body};
    this->_recv_handshake(client_hello);
    if (client_hello.msg_type != TLS::HandshakeType::CLIENT_HELLO)
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::HANDSHAKE_FAILURE);
    
    this->_pending_state.entity = this->_current_state.entity;

    this->_protocol_version.major = client_body.read_std_ubyte();
    this->_protocol_version.minor = client_body.read_std_ubyte();
    if (this->_protocol_version.major != TLS::VERSION_1_2.major || this->_protocol_version.minor != TLS::VERSION_1_2.minor)
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::HANDSHAKE_FAILURE);
    server_body.write_std_ubyte(this->_protocol_version.major);
    server_body.write_std_ubyte(this->_protocol_version.minor);                         // server_version

    client_body.read((char*)&this->_pending_state.client_random, TLS::RANDOM_LENGTH);
    uint32_t gmt_unix_time = Utility::htonl(std::time(0));
    std::memcpy(&this->_pending_state.server_random, &gmt_unix_time, sizeof(uint32_t)); // random.gmt_unix_time
    TLS::IV((uint8_t*)&this->_pending_state.server_random + sizeof(uint32_t), TLS::RANDOM_LENGTH - sizeof(uint32_t));
    server_body.write((char*)&this->_pending_state.server_random, TLS::RANDOM_LENGTH);  // random.random_bytes

    client_body.ignore(client_body.read_std_ubyte());
    server_body.write_std_ubyte(0);                     // session_id 0 -> no resumption

    // Read from client_body, write into server_body. These were reading the
    // client's offer out of the reply being built, which is empty at this point,
    // so no cipher suite and no compression method could ever be agreed.
    bool cipher_suite_found = false;
    uint16_t cipher_suites_count = client_body.read_std_ushort() / sizeof(TLS::CipherSuite);
    for (uint16_t i = 0; i < cipher_suites_count; i++) {
      TLS::CipherSuite client_suite {client_body.read_std_ubyte(), client_body.read_std_ubyte()};
      for (TLS::CipherSuite& server_suite : this->_handshake_params.cipher_suites) {
	if (client_suite.revision == server_suite.revision && client_suite.suite_id == server_suite.suite_id) {
	  TLS::cipher_suite(server_suite, this->_pending_state);
	  server_body.write_std_ubyte(client_suite.revision);
	  server_body.write_std_ubyte(client_suite.suite_id);	  
	  cipher_suite_found = true;
	  break;
	}
      }
      if (cipher_suite_found) break;
    }
    if (!cipher_suite_found) {
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::HANDSHAKE_FAILURE);  // cipher_suites
    }
    
    bool compression_found = false;
    uint8_t client_compressions_count = client_body.read_std_ubyte();
    for (uint8_t i = 0; i < client_compressions_count; i++) {
      TLS::CompressionMethod client_compression = (TLS::CompressionMethod)client_body.read_std_ubyte();
      for (TLS::CompressionMethod server_compression : this->_handshake_params.compression_methods) {
	if (client_compression == server_compression) {
	  this->_pending_state.compression_algorithm = server_compression;
	  server_body.write_std_ubyte((uint8_t)client_compression);
	  compression_found = true;
	  break;
	}
      }
      if (compression_found) break;
    }
    if (!compression_found) {
      this->_pending_state.compression_algorithm = TLS::CompressionMethod::NONE;
      server_body.write_std_ubyte((uint8_t)TLS::CompressionMethod::NONE);         // compression_methods
    }

    std::vector<TLS::Extension> server_extensions;                                // extensions
    uint16_t extensions_length = client_body.read_std_ushort();
    if (!client_body.eof() && this->_handshake_params.extensions.size() > 0) {
      uint8_t extensions_buffer[extensions_length];
      client_body.read((char*)extensions_buffer, extensions_length);
      arraystream extensions(extensions_buffer, extensions_length);

      while (extensions_length > 0) {
	TLS::Extension client_extension;
        client_extension.extension_type = (TLS::ExtensionType)client_body.read_std_ushort();
	extensions_length -= sizeof(TLS::ExtensionType);
	for (TLS::Extension& server_extension : this->_handshake_params.extensions) {
	  if (client_extension.extension_type == server_extension.extension_type) {
	    uint16_t data_length = client_body.read_std_ushort();
	    client_extension.extension_data = new uint8_t[data_length];
	    client_body.read((char*)client_extension.extension_data, data_length);
	    extensions_length -= sizeof(uint16_t) + data_length;
	    bufferstream supported_extension;
	    TLS::check_extension(server_extension, client_extension, supported_extension);
	    supported_extension.read(server_body, supported_extension.tellp());
	  }
	}
      }

      extensions_length = extensions.tellp();
      server_body.write_std_ushort(extensions_length);
      extensions.read(server_body, extensions_length);
    }
    
    TLS::Handshake server_hello {TLS::HandshakeType::SERVER_HELLO, server_body.tellp(), server_body};
    this->_send_handshake(server_hello);
  }

  // The server's side of a mutually authenticated exchange:
  //
  //   <- ClientHello                ServerHello ->
  //                                Certificate ->
  //                         CertificateRequest ->
  //                           ServerHelloDone  ->
  //   <- Certificate
  //   <- ClientKeyExchange
  //   <- CertificateVerify
  //   <- ChangeCipherSpec, Finished
  //                    ChangeCipherSpec, Finished ->
  //
  // The client is asked for a certificate unconditionally. That is the point of
  // the arrangement: nobody talks to this server without one the owner issued.
  void tlsbuf::_server_handshake()
  {
    this->_transcript.string("");
    this->_peer_subject.clear();

    this->_server_hello();
    this->_send_certificate();

    {
      // CertificateRequest. One type, RSA signing, and no acceptable authority
      // named -- there is only ever one, and the peer already has it.
      bufferstream body;
      body.write_std_ubyte(1);
      body.write_std_ubyte((uint8_t)TLS::ClientCertificateType::RSA_SIGN);
      body.write_std_ushort(0);                       // certificate_authorities
      TLS::Handshake message {TLS::HandshakeType::CERTIFICATE_REQUEST, body.length(), body};
      this->_send_handshake(message);
    }

    {
      bufferstream body;
      TLS::Handshake message {TLS::HandshakeType::SERVER_HELLO_DONE, 0, body};
      this->_send_handshake(message);
    }

    bufferstream peer_certificate;
    this->_recv_certificate(peer_certificate);
    this->_check_peer_certificate(peer_certificate);

    // ClientKeyExchange: the pre master secret, encrypted to this server's key.
    bufferstream pre_master_secret;
    {
      bufferstream body;
      TLS::Handshake message {(TLS::HandshakeType)0, 0, body};
      this->_recv_handshake(message);
      if (message.msg_type != TLS::HandshakeType::CLIENT_KEY_EXCHANGE)
	this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);

      const uint16_t length = body.read_std_ushort();
      bufferstream encrypted;
      body.read(encrypted, length);

      bufferstream key;
      this->_credential(this->_handshake_params.credentials.private_key, key);
      try {
	X509::decrypt(key, this->_handshake_params.credentials.private_key_cipher,
		      encrypted, pre_master_secret);
      } catch (const std::exception&) {
	this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::DECRYPT_ERROR);
      }
    }

    // CertificateVerify, over everything said up to here. Without it a client
    // could present somebody else's certificate: encrypting to the server's key
    // proves nothing about who is doing the encrypting.
    {
      uint8_t digest[SHA::size(SHA::SHA256)];
      this->_transcript_hash(digest);

      bufferstream body;
      TLS::Handshake message {(TLS::HandshakeType)0, 0, body};
      this->_recv_handshake(message);
      if (message.msg_type != TLS::HandshakeType::CERTIFICATE_VERIFY)
	this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);

      body.read_std_ubyte();                          // hash algorithm
      body.read_std_ubyte();                          // signature algorithm
      const uint16_t length = body.read_std_ushort();

      bufferstream signature, signed_digest, certificate;
      body.read(signature, length);
      signed_digest.write((char*)digest, (std::streamsize)sizeof(digest));
      peer_certificate.read(certificate, 0, peer_certificate.length());

      if (!X509::verify(certificate, "SHA-256", signed_digest, signature))
	this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::DECRYPT_ERROR);
    }

    this->_derive_keys(pre_master_secret);

    this->_recv_change_cipher_spec();
    this->_recv_finished("client finished");

    this->_send_change_cipher_spec();
    this->_send_finished("server finished");
  }

  void tlsbuf::_client_hello()
  {
    bufferstream client_body;

    this->_pending_state.entity = this->_current_state.entity;

    client_body.write_std_ubyte(this->_handshake_params.protocol_version.major);
    client_body.write_std_ubyte(this->_handshake_params.protocol_version.minor);        // client_version

    uint32_t gmt_unix_time = Utility::htonl(std::time(0));
    std::memcpy(&this->_pending_state.client_random, &gmt_unix_time, sizeof(uint32_t)); // random.gmt_unix_time
    TLS::IV((uint8_t*)&this->_pending_state.client_random + sizeof(uint32_t), TLS::RANDOM_LENGTH - sizeof(uint32_t));
    client_body.write((char*)&this->_pending_state.client_random, TLS::RANDOM_LENGTH);  // random.random_bytes

    client_body.write_std_ubyte(this->_handshake_params.session_id.size());
    client_body.write((char*)this->_handshake_params.session_id.data(),
		 this->_handshake_params.session_id.size());                         // session_id

    client_body.write_std_ushort(this->_handshake_params.cipher_suites.size() * sizeof(TLS::CipherSuite));
    client_body.write((char*)this->_handshake_params.cipher_suites.data(),
		 this->_handshake_params.cipher_suites.size() * sizeof(TLS::CipherSuite)); // cipher_suites

    client_body.write_std_ubyte(this->_handshake_params.compression_methods.size());
    client_body.write((char*)this->_handshake_params.compression_methods.data(),
		 this->_handshake_params.compression_methods.size());                // compression_methods

    if (this->_handshake_params.extensions.size() > 0) {                             // extensions
      bufferstream extensions;
      for (TLS::Extension& extension : this->_handshake_params.extensions) {
	switch (extension.extension_type) {                                          // extension_data
	case TLS::ExtensionType::SIGNATURE_ALGORITHMS:
	  break;
	default:
	  throw TLSException("unsupported extention");
	}
        extensions.write_std_ushort((uint16_t)extension.extension_type);
	uint16_t data_length = Utility::ntohs(*((uint16_t*)extension.extension_data));
	client_body.write((char*)extension.extension_data, sizeof(uint16_t));
	client_body.write((char*)extension.extension_data + sizeof(uint16_t), data_length);
	delete[] extension.extension_data;
      }

      uint16_t extensions_length = extensions.tellp();
      client_body.write_std_ushort(extensions_length);
      extensions.read(client_body, extensions_length);
    }

    TLS::Handshake client_hello {TLS::HandshakeType::CLIENT_HELLO, client_body.tellp(), client_body};
    this->_send_handshake(client_hello);
  }

  void tlsbuf::_client_handshake()
  {
    this->_transcript.string("");
    this->_peer_subject.clear();

    this->_client_hello();

    // ServerHello, and the state it settled on.
    {
      bufferstream body;
      TLS::Handshake message {(TLS::HandshakeType)0, 0, body};
      this->_recv_handshake(message);
      if (message.msg_type != TLS::HandshakeType::SERVER_HELLO)
	this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);

      this->_pending_state.entity = this->_current_state.entity;
      this->_pending_state.prf_algorithm = TLS::PRFAlgorithm::TLS_PRF_SHA256;

      this->_protocol_version.major = body.read_std_ubyte();
      this->_protocol_version.minor = body.read_std_ubyte();

      body.read((char*)&this->_pending_state.server_random, TLS::RANDOM_LENGTH);
      body.ignore(body.read_std_ubyte());             // session_id

      TLS::CipherSuite chosen {body.read_std_ubyte(), body.read_std_ubyte()};
      TLS::cipher_suite(chosen, this->_pending_state);
      this->_pending_state.compression_algorithm = (TLS::CompressionMethod)body.read_std_ubyte();
    }

    bufferstream peer_certificate;
    this->_recv_certificate(peer_certificate);
    this->_check_peer_certificate(peer_certificate);

    // CertificateRequest, then ServerHelloDone.
    {
      bufferstream body;
      TLS::Handshake message {(TLS::HandshakeType)0, 0, body};
      this->_recv_handshake(message);
      if (message.msg_type != TLS::HandshakeType::CERTIFICATE_REQUEST)
	this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);
    }
    {
      bufferstream body;
      TLS::Handshake message {(TLS::HandshakeType)0, 0, body};
      this->_recv_handshake(message);
      if (message.msg_type != TLS::HandshakeType::SERVER_HELLO_DONE)
	this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);
    }

    this->_send_certificate();

    // The pre master secret: the version this end offered, then 46 random
    // octets, encrypted to the key the server's certificate names.
    bufferstream pre_master_secret;
    {
      pre_master_secret.write_std_ubyte(this->_handshake_params.protocol_version.major);
      pre_master_secret.write_std_ubyte(this->_handshake_params.protocol_version.minor);
      uint8_t random[46];
      TLS::IV(random, sizeof(random));
      pre_master_secret.write((char*)random, (std::streamsize)sizeof(random));

      bufferstream to_encrypt, encrypted, certificate;
      pre_master_secret.read(to_encrypt, 0, pre_master_secret.length());
      peer_certificate.read(certificate, 0, peer_certificate.length());
      X509::encrypt(certificate, to_encrypt, encrypted);

      bufferstream body;
      body.write_std_ushort((uint16_t)encrypted.length());
      encrypted.read(body, 0, encrypted.length());

      TLS::Handshake message {TLS::HandshakeType::CLIENT_KEY_EXCHANGE, body.length(), body};
      this->_send_handshake(message);
    }

    // CertificateVerify: this end signs the conversation so far, which is what
    // proves it holds the key its certificate names.
    {
      uint8_t digest[SHA::size(SHA::SHA256)];
      this->_transcript_hash(digest);

      bufferstream key, to_sign, signature;
      this->_credential(this->_handshake_params.credentials.private_key, key);
      to_sign.write((char*)digest, (std::streamsize)sizeof(digest));
      X509::sign(key, this->_handshake_params.credentials.private_key_cipher,
		 "SHA-256", to_sign, signature);

      bufferstream body;
      body.write_std_ubyte((uint8_t)TLS::HashAlgorithm::SHA256);
      body.write_std_ubyte((uint8_t)TLS::SignatureAlgorithm::RSA);
      body.write_std_ushort((uint16_t)signature.length());
      signature.read(body, 0, signature.length());

      TLS::Handshake message {TLS::HandshakeType::CERTIFICATE_VERIFY, body.length(), body};
      this->_send_handshake(message);
    }

    this->_derive_keys(pre_master_secret);

    this->_send_change_cipher_spec();
    this->_send_finished("client finished");

    this->_recv_change_cipher_spec();
    this->_recv_finished("server finished");
  }

  tlsbuf* tlsbuf::open(TLS::ConnectionEnd entity, TLS::HandshakeParameters params, Socket::handle_t handle, bool blocking_mode, int timeout)
  {
    std::memset(&this->_current_state, 0x00, sizeof(TLS::SecurityParameters));
    std::memset(&this->_pending_state, 0x00, sizeof(TLS::SecurityParameters));
    this->_current_state.entity = entity;
    if (params.protocol_version.major != TLS::VERSION_1_2.major || params.protocol_version.minor != TLS::VERSION_1_2.minor)
      throw TLSException("unsupported protocol version");
    this->_handshake_params = params;

    // Application data needs somewhere to gather before it becomes a record.
    if (this->_buffer == nullptr)
      this->setbuf(new char_type[tlsbuf::BUFFER_SIZE], tlsbuf::BUFFER_SIZE);

    this->_tcpstream.open(handle, blocking_mode, timeout);
    if (entity == TLS::ConnectionEnd::SERVER)
      this->_server_handshake();
    else
      this->_client_handshake();
    return this;
  }

  tlsbuf* tlsbuf::open(TLS::HandshakeParameters params, std::string node, std::string service, bool blocking_mode, int timeout)
  {
    std::memset(&this->_current_state, 0x00, sizeof(TLS::SecurityParameters));
    std::memset(&this->_pending_state, 0x00, sizeof(TLS::SecurityParameters));
    this->_current_state.entity = TLS::ConnectionEnd::CLIENT;
    if (params.protocol_version.major != TLS::VERSION_1_2.major || params.protocol_version.minor != TLS::VERSION_1_2.minor)
      throw TLSException("unsupported protocol version");
    this->_handshake_params = params;

    // Application data needs somewhere to gather before it becomes a record.
    if (this->_buffer == nullptr)
      this->setbuf(new char_type[tlsbuf::BUFFER_SIZE], tlsbuf::BUFFER_SIZE);

    this->_tcpstream.open(node, service, blocking_mode, timeout);
    this->_client_handshake();
    return this;
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
    arraystream plain_text(this->_buffer, this->_length);

    TLS::Record record {(TLS::ContentType)0, {0, 0}, 0, plain_text};
    
    this->_recv_record(record);
    if (record.type != TLS::ContentType::APPLICATION_DATA)
      this->_alert(TLS::AlertLevel::FATAL, TLS::AlertDescription::UNEXPECTED_MESSAGE);
    
    if (!this->_tcpstream.good()) {
      this->setg(nullptr, nullptr, nullptr); 
      return traits_type::eof();
    }

    this->setg(this->_buffer, this->_buffer, this->_buffer + plain_text.tellp());
    return traits_type::to_int_type(*this->_buffer);
  }

  tlsbuf::int_type tlsbuf::overflow(int_type ch)
  {
    std::streamsize plain_text_length = this->pptr() - this->pbase();

    // Nothing pending is not a failure, and an empty record is not worth
    // sending: a flush on an idle connection would otherwise emit one.
    if (plain_text_length == 0) {
      if (traits_type::eq_int_type(ch, traits_type::eof()))
	return traits_type::not_eof(ch);
    }

    arraystream     plain_text(this->_buffer, plain_text_length);

    TLS::Record record {TLS::ContentType::APPLICATION_DATA, this->_protocol_version, plain_text_length, plain_text};
    
    this->_send_record(record);
    
    if (!this->_tcpstream.good()) {
      this->setp(nullptr, nullptr);      
      return traits_type::eof();
    }

    if (!traits_type::eq_int_type(ch, traits_type::eof())) *this->_buffer = ch;
    this->setp(this->_buffer, this->_buffer + this->_length);
    return traits_type::to_int_type(0);
  }

  tlsbuf::int_type tlsbuf::pbackfail(int_type)
  {
    return traits_type::eof();
  }

  void tlsbuf::renegotiate()
  {
    if (this->_current_state.entity == TLS::ConnectionEnd::CLIENT) throw TLSException("only server could start renegotiation process");

    bufferstream plain_text;
    plain_text.write_std_ubyte((uint8_t)TLS::HandshakeType::HELLO_REQUEST);
    plain_text.fill_n(3, 0x00);
    
    TLS::Record record {TLS::ContentType::HANDSHAKE, this->_protocol_version, plain_text.tellp(), plain_text};
    
    this->_send_record(record);

    this->_server_handshake();
  }

  bool tlsbuf::is_open() const
  {
    return this->_tcpstream.is_open();
  }

  tlsbuf* tlsbuf::close()
  {
    // close_notify is a warning, not a failure. Sending it at FATAL made _alert
    // throw on the way out of every clean shutdown, so closing a healthy
    // connection raised an exception. A peer that has already gone is not an
    // error either -- there is nobody left to tell.
    if (this->_tcpstream.is_open()) {
      try {
	this->_alert(TLS::AlertLevel::WARNING, TLS::AlertDescription::CLOSE_NOTIFY);
      } catch (...) {
      }
    }

    this->_tcpstream.close();
    return this;
  }

  tlsbuf::~tlsbuf()
  {
    this->_tcpstream.close();

    delete[] this->_buffer;
    delete[] this->client_write_MAC_key;
    delete[] this->server_write_MAC_key;
    delete[] this->client_write_key;
    delete[] this->server_write_key;
    delete[] this->client_write_IV;
    delete[] this->server_write_IV;
  }

}
