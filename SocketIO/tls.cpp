#include "tls.hpp"
#include <vector>
#include "tlsexception.hpp"
#include "shahelper.hpp"
#include <cstring>
#include "utility.hpp"
#include "zlibhelper.hpp"
#include "aes.hpp"
#include "bufferstream.hpp"
#include <cassert>


namespace Zigurat
{

  const TLS::ProtocolVersion TLS::VERSION_1_2 = {3, 3};

  bool operator==(const TLS::CipherSuite& self, const TLS::CipherSuite& other)
  {
    return (self.revision == other.revision && self.suite_id == other.suite_id);
  }

  const TLS::CipherSuite TLS::TLS_NULL_WITH_NULL_NULL         = {0x00,0x00};
  const TLS::CipherSuite TLS::TLS_RSA_WITH_NULL_SHA           = {0x00,0x02};
  const TLS::CipherSuite TLS::TLS_RSA_WITH_NULL_SHA256        = {0x00,0x3B};
  const TLS::CipherSuite TLS::TLS_RSA_WITH_AES_128_CBC_SHA    = {0x00,0x2F};
  const TLS::CipherSuite TLS::TLS_RSA_WITH_AES_256_CBC_SHA    = {0x00,0x35};
  const TLS::CipherSuite TLS::TLS_RSA_WITH_AES_128_CBC_SHA256 = {0x00,0x3C};
  const TLS::CipherSuite TLS::TLS_RSA_WITH_AES_256_CBC_SHA256 = {0x00,0x3D};      

  const TLS::SignatureAndHashAlgorithm TLS::SIG_RSA_SHA1   = {HashAlgorithm::SHA1,   SignatureAlgorithm::RSA};
  const TLS::SignatureAndHashAlgorithm TLS::SIG_RSA_SHA224 = {HashAlgorithm::SHA224, SignatureAlgorithm::RSA};
  const TLS::SignatureAndHashAlgorithm TLS::SIG_RSA_SHA256 = {HashAlgorithm::SHA256, SignatureAlgorithm::RSA};
  const TLS::SignatureAndHashAlgorithm TLS::SIG_RSA_SHA384 = {HashAlgorithm::SHA384, SignatureAlgorithm::RSA};
  const TLS::SignatureAndHashAlgorithm TLS::SIG_RSA_SHA512 = {HashAlgorithm::SHA512, SignatureAlgorithm::RSA};

  // RFC 5246 5, P_hash:
  //
  //     A(0) = seed
  //     A(i) = HMAC(secret, A(i-1))
  //     P_hash = HMAC(secret, A(1) + seed) + HMAC(secret, A(2) + seed) + ...
  //
  // Two things were wrong here. The scratch buffer was declared uint8_t
  // buffer[length] with length still zero, so every HMAC wrote thirty two octets
  // into a nothing-sized stack array -- deriving keys corrupted the stack of
  // whoever asked for them. And the construction was not P_hash: it chained on
  // the output block rather than on A(i), and never appended the seed, so the
  // result was neither the PRF the specification defines nor a sound one.
  void TLS::P_SHA256(const uint8_t* secret, size_t secret_length,
		     const uint8_t* seed,   size_t seed_length,
		           uint8_t* digest, size_t digest_length)
  {
    const size_t hash_size = SHA::size(SHA::SHA256);

    std::vector<uint8_t> a(seed, seed + seed_length);          // A(0) = seed
    std::vector<uint8_t> input(hash_size + seed_length);
    std::vector<uint8_t> block(hash_size);
    std::vector<uint8_t> next(hash_size);

    size_t produced = 0;
    while (produced < digest_length) {

      SHA::hmac(SHA::SHA256, secret, secret_length, a.data(), a.size(), next.data());
      a.assign(next.begin(), next.end());                      // A(i)

      std::memcpy(input.data(), a.data(), hash_size);
      std::memcpy(input.data() + hash_size, seed, seed_length);
      SHA::hmac(SHA::SHA256, secret, secret_length, input.data(), input.size(), block.data());

      const size_t take = Utility::min(hash_size, digest_length - produced);
      std::memcpy(digest + produced, block.data(), take);
      produced += take;
    }
  }

  void TLS::PRF(PRFAlgorithm algorithm,
		const uint8_t* secret, size_t secret_length,
		const uint8_t* label,  size_t label_length,
		const uint8_t* seed,   size_t seed_length,
	              uint8_t* digest, size_t digest_length)
  {
    size_t  length = label_length + seed_length;
    uint8_t buffer[length];
    std::memcpy(buffer, label, label_length);
    std::memcpy(buffer + label_length, seed, seed_length);

    switch (algorithm) {
    case PRFAlgorithm::TLS_PRF_SHA256:
      TLS::P_SHA256(secret, secret_length, buffer, length, digest, digest_length);
      break;
    default:
      throw TLSException("unknown PRF algorithm");
    }
  }

  void TLS::calculate_keys(const SecurityParameters& parameters,
			   uint8_t *client_write_MAC_key, uint8_t *server_write_MAC_key,
			   uint8_t *client_write_key,     uint8_t *server_write_key,
			   uint8_t *client_write_IV,      uint8_t *server_write_IV)
  {
    size_t  length = TLS::RANDOM_LENGTH + TLS::RANDOM_LENGTH;
    uint8_t buffer[length];
    std::memcpy(buffer, parameters.server_random, TLS::RANDOM_LENGTH);
    std::memcpy(buffer + TLS::RANDOM_LENGTH, parameters.client_random, TLS::RANDOM_LENGTH);
        
    size_t  digest_length = (parameters.mac_key_length * 2) + (parameters.enc_key_length * 2) + (parameters.fixed_iv_length * 2);
    uint8_t digest[digest_length];
    
    TLS::PRF(parameters.prf_algorithm,
	     parameters.master_secret,  TLS::MASTER_SECRET_LENGTH,
	     (uint8_t*)"key expansion", 13,
	     buffer,                    length,
	     digest,                    digest_length);

    uint8_t *cursor = digest;

    std::memcpy(client_write_MAC_key, cursor, parameters.mac_key_length);
    cursor += parameters.mac_key_length;
    std::memcpy(server_write_MAC_key, cursor, parameters.mac_key_length);
    cursor += parameters.mac_key_length;

    std::memcpy(client_write_key, cursor, parameters.enc_key_length);
    cursor += parameters.enc_key_length;
    std::memcpy(server_write_key, cursor, parameters.enc_key_length);
    cursor += parameters.enc_key_length;

    std::memcpy(client_write_IV, cursor, parameters.fixed_iv_length);
    cursor += parameters.fixed_iv_length;
    std::memcpy(server_write_IV, cursor, parameters.fixed_iv_length);
  }

  void TLS::MAC(MACAlgorithm algorithm,
		const uint8_t* secret, size_t secret_length,
	        uint64_t sequence_number,
		ContentType type, ProtocolVersion version,
		binarystream& compressed, uint16_t compressed_length,
		uint8_t* mac)
  {
    size_t   length = secret_length + sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(uint16_t) + compressed_length; 
    uint8_t  buffer[length];
    uint8_t* cursor = buffer;
    
    std::memcpy(cursor, secret, secret_length);                  // MAC_write_key
    sequence_number = Utility::htonll(sequence_number);
    cursor += secret_length;
    std::memcpy(cursor, &sequence_number, sizeof(uint64_t));     // seq_number
    cursor += sizeof(uint64_t);
    *cursor++ = (uint8_t)type;                                   // TLSCompressed.type
    *cursor++ = version.major;
    *cursor++ = version.minor;                                   // TLSCompressed.version
    compressed_length = Utility::htons(compressed_length);
    std::memcpy(cursor, &compressed_length, sizeof(uint16_t));   // TLSCompressed.length
    cursor += sizeof(uint16_t);
    compressed.read((char*)cursor, 0, compressed_length);        // TLSCompressed.fragment 
    
    switch (algorithm) {
    case MACAlgorithm::HMAC_SHA1:
      SHA::hmac(SHA::SHA1  , secret, secret_length, buffer, length, mac);
      break;
    case MACAlgorithm::HMAC_SHA256:
      SHA::hmac(SHA::SHA256, secret, secret_length, buffer, length, mac);
      break;
    case MACAlgorithm::HMAC_SHA384:
      SHA::hmac(SHA::SHA384, secret, secret_length, buffer, length, mac);
      break;
    case MACAlgorithm::HMAC_SHA512:
      SHA::hmac(SHA::SHA512, secret, secret_length, buffer, length, mac);
      break;
    default:
      throw TLSException("unsupported MAC algorithm");
    }
  }

  void TLS::IV(uint8_t* buffer, uint8_t length)
  {
    std::srand(std::time(nullptr));
    int secret[3];
    secret[0] = std::rand();
    secret[1] = std::rand();
    secret[2] = std::rand();

    std::srand(std::time(nullptr));
    int seed[3];
    seed[0] = std::rand();
    seed[1] = std::rand();
    seed[2] = std::rand();

    TLS::PRF(PRFAlgorithm::TLS_PRF_SHA256, (uint8_t*)&secret, 12, (uint8_t*)"record IV", 9, (uint8_t*)&seed, 12, buffer, length);
  }

  void TLS::uint24(uint32_t d, binarystream& stream)
  {
    uint8_t uint24[3];
    uint24[0] = d >> 16;
    uint24[1] = d >> 8;
    uint24[2] = d;
    stream.write((char*)&uint24, 3);
  }

  uint32_t TLS::uint24(binarystream& stream)
  {
    uint8_t uint24[3];
    stream.read((char*)&uint24, 3);
    uint32_t d = uint24[0];
    d |= uint24[1];
    d |= uint24[2];
    return d;
  }

  void TLS::cipher_suite(const CipherSuite& suite, SecurityParameters& params)
  {
    if (suite == TLS::TLS_NULL_WITH_NULL_NULL) {
      params.bulk_cipher_algorithm = BulkCipherAlgorithm::NONE;
      params.cipher_type = CipherType::STREAM;
      params.enc_key_length = 0;
      params.block_length = 0;
      params.fixed_iv_length = 0;
      params.record_iv_length = 0;
      params.mac_algorithm = MACAlgorithm::NONE;
      params.mac_length = 0;
      params.mac_key_length = 0;
    } else if (suite == TLS::TLS_RSA_WITH_NULL_SHA) {
      params.bulk_cipher_algorithm = BulkCipherAlgorithm::NONE;
      params.cipher_type = CipherType::STREAM;
      params.enc_key_length = 0;
      params.block_length = 0;
      params.fixed_iv_length = 0;
      params.record_iv_length = 0;
      params.mac_algorithm = MACAlgorithm::HMAC_SHA1;
      params.mac_length = 20;
      params.mac_key_length = 20;
    } else if (suite == TLS::TLS_RSA_WITH_NULL_SHA256) {
      params.bulk_cipher_algorithm = BulkCipherAlgorithm::NONE;
      params.cipher_type = CipherType::STREAM;
      params.enc_key_length = 0;
      params.block_length = 0;
      params.fixed_iv_length = 0;
      params.record_iv_length = 0;
      params.mac_algorithm = MACAlgorithm::HMAC_SHA256;
      params.mac_length = 32;
      params.mac_key_length = 32;
    } else if (suite == TLS::TLS_RSA_WITH_AES_128_CBC_SHA) {
      params.bulk_cipher_algorithm = BulkCipherAlgorithm::AES;
      params.cipher_type = CipherType::BLOCK;
      params.enc_key_length = 16;
      params.block_length = 16;
      params.fixed_iv_length = 0;
      params.record_iv_length = 16;
      params.mac_algorithm = MACAlgorithm::HMAC_SHA1;
      params.mac_length = 20;
      params.mac_key_length = 20;
    } else if (suite == TLS::TLS_RSA_WITH_AES_256_CBC_SHA) {
      params.bulk_cipher_algorithm = BulkCipherAlgorithm::AES;
      params.cipher_type = CipherType::BLOCK;
      params.enc_key_length = 32;
      params.block_length = 16;
      params.fixed_iv_length = 0;
      params.record_iv_length = 16;
      params.mac_algorithm = MACAlgorithm::HMAC_SHA1;
      params.mac_length = 20;
      params.mac_key_length = 20;
    } else if (suite == TLS::TLS_RSA_WITH_AES_128_CBC_SHA256) {
      params.bulk_cipher_algorithm = BulkCipherAlgorithm::AES;
      params.cipher_type = CipherType::BLOCK;
      params.enc_key_length = 16;
      params.block_length = 16;
      params.fixed_iv_length = 0;
      params.record_iv_length = 16;
      params.mac_algorithm = MACAlgorithm::HMAC_SHA256;
      params.mac_length = 32;
      params.mac_key_length = 32;
    } else if (suite == TLS::TLS_RSA_WITH_AES_256_CBC_SHA256) {
      params.bulk_cipher_algorithm = BulkCipherAlgorithm::AES;
      params.cipher_type = CipherType::BLOCK;
      params.enc_key_length = 32;
      params.block_length = 16;
      params.fixed_iv_length = 0;
      params.record_iv_length = 16;
      params.mac_algorithm = MACAlgorithm::HMAC_SHA256;
      params.mac_length = 32;
      params.mac_key_length = 32;
    } else {
      throw TLSException("unsupported cipher suite");
    }
  }

  TLS::Extension TLS::signature_algorithm_extention(std::vector<SignatureAndHashAlgorithm> algorithms)
  {
    bufferstream data;
    data.write_std_ushort(sizeof(uint16_t) + (algorithms.size() * sizeof(SignatureAndHashAlgorithm)));
    data.write_std_ushort(algorithms.size() * sizeof(SignatureAndHashAlgorithm));
    for (const SignatureAndHashAlgorithm& algorithm : algorithms) {
      data.write_std_ubyte((uint8_t)algorithm.hash);
      data.write_std_ubyte((uint8_t)algorithm.signature);
    }
    uint16_t length = data.tellp();
    uint8_t* buffer = new uint8_t[length];
    data.read((char*)buffer, length);
    return Extension {ExtensionType::SIGNATURE_ALGORITHMS, buffer};
  }

  void TLS::check_extension(const Extension& server_extension, const Extension& client_extension, binarystream& extension_buffer)
  {
    assert(server_extension.extension_type == client_extension.extension_type);
    uint8_t *server_cursor = server_extension.extension_data;
    uint8_t *client_cursor = client_extension.extension_data;
    uint16_t server_length = Utility::ntohs(*((uint16_t*)server_cursor));
    uint16_t client_length = Utility::ntohs(*((uint16_t*)client_cursor));
    server_cursor += sizeof(uint16_t);
    client_cursor += sizeof(uint16_t);

    switch (server_extension.extension_type) {
    case TLS::ExtensionType::SIGNATURE_ALGORITHMS:
      {
	if (server_length > 0 && client_length > 0) {
	  extension_buffer.write_std_ushort((uint16_t)TLS::ExtensionType::SIGNATURE_ALGORITHMS);
    
	  uint16_t server_count = Utility::ntohs(*((uint16_t*)server_cursor));
	  uint16_t client_count = Utility::ntohs(*((uint16_t*)client_cursor));
	  server_cursor += sizeof(uint16_t);
	  client_cursor += sizeof(uint16_t);

	  std::vector<SignatureAndHashAlgorithm> supported;
	  
	  for (uint16_t i = 0; i < server_count; i++) {
	    for (uint16_t j = 0; j < client_count; j++) {
	      if (*(server_cursor + (i * sizeof(SignatureAndHashAlgorithm))) ==
		  *(client_cursor + (j * sizeof(SignatureAndHashAlgorithm))) &&
		  *(server_cursor + (i * sizeof(SignatureAndHashAlgorithm)) + sizeof(uint8_t)) ==
		  *(client_cursor + (j * sizeof(SignatureAndHashAlgorithm)) + sizeof(uint8_t))) {
		supported.push_back(SignatureAndHashAlgorithm {
		    (HashAlgorithm)*(server_cursor + (i * sizeof(SignatureAndHashAlgorithm))),
		    (SignatureAlgorithm)*(server_cursor + (i * sizeof(SignatureAndHashAlgorithm)) + sizeof(uint8_t))});
	      }
	    }
	  }

	  extension_buffer.write_std_ushort((uint16_t)(supported.size() * sizeof(SignatureAndHashAlgorithm)));
	  for (const SignatureAndHashAlgorithm& algorithm : supported) {
	    extension_buffer.write_std_ubyte((uint8_t)algorithm.hash);
	    extension_buffer.write_std_ubyte((uint8_t)algorithm.signature);
	  }
	}
      }
      break;
    default:
      throw TLSException("unsupported extention");
    }
  }

}
