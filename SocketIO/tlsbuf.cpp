#include "tlsbuf.hpp"
#include <cstring>
#include "utility.hpp"
#include "bufferstream.hpp"
#include "arraystream.hpp"
#include "tlsexception.hpp"
#include "zlibhelper.hpp"
#include "aes.hpp"
#include "shahelper.hpp"
#include <cassert>


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

      if (record.type == TLS::ContentType::ALERT) {
	TLS::AlertLevel level = (TLS::AlertLevel)record.fragment.read_std_ubyte();
	TLS::AlertDescription description = (TLS::AlertDescription)record.fragment.read_std_ubyte();
	if (level == TLS::AlertLevel::FATAL)
	  throw TLSException("fatal alert " + std::to_string((uint8_t)description));
	record.type = (TLS::ContentType)0;
	record.version = {0, 0};
	record.length = 0;
	record.fragment.seekg(-record.length, std::ios_base::cur);
	record.fragment.seekp(-record.length, std::ios_base::cur);
	continue;
      } else if (record.type == TLS::ContentType::HANDSHAKE && params.entity == TLS::ConnectionEnd::CLIENT) {
	TLS::HandshakeType msg_type = (TLS::HandshakeType)record.fragment.read_std_ubyte();
	if (msg_type == TLS::HandshakeType::HELLO_REQUEST) {
	  record.type = (TLS::ContentType)0;
	  record.version = {0, 0};
	  record.length = 0;
	  record.fragment.seekg(-record.length, std::ios_base::cur);
	  record.fragment.seekp(-record.length, std::ios_base::cur);
	  this->_client_handshake();
	  continue;
	} else {
	  record.fragment.seekg(-record.length, std::ios_base::cur);
	  break;
	}
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
    assert(record.type == TLS::ContentType::HANDSHAKE);

    handshake.msg_type = (TLS::HandshakeType)plain_text.read_std_ubyte();
    handshake.length   = TLS::uint24(plain_text);

    // What the first record carried beyond the four octet header. This was
    // adding the header rather than subtracting it, so the count ran two octets
    // over the body on every message and a fragmented one was never reassembled.
    std::streamsize count = record.length - (sizeof(uint8_t) * 4);
    plain_text.read(handshake.body, count);

    while (count < handshake.length) {
      this->_recv_record(record);
      assert(record.type == TLS::ContentType::HANDSHAKE);
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

  void tlsbuf::_server_handshake()
  {
    this->_server_hello();
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
    this->_client_hello();
  }

  tlsbuf* tlsbuf::open(TLS::ConnectionEnd entity, TLS::HandshakeParameters params, Socket::handle_t handle, bool blocking_mode, int timeout)
  {
    std::memset(&this->_current_state, 0x00, sizeof(TLS::SecurityParameters));
    std::memset(&this->_pending_state, 0x00, sizeof(TLS::SecurityParameters));
    this->_current_state.entity = entity;
    if (params.protocol_version.major != TLS::VERSION_1_2.major || params.protocol_version.minor != TLS::VERSION_1_2.minor)
      throw TLSException("unsupported protocol version");
    this->_handshake_params = params;
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

  int tlsbuf::sync()
  {
    if (this->overflow(traits_type::eof()) == traits_type::eof()) return -1;
    if (this->underflow() == traits_type::eof()) return -1;

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
    assert(record.type == TLS::ContentType::APPLICATION_DATA);
    
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
  }

}
