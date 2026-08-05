
#ifndef __TLS_HPP__
#define __TLS_HPP__


#include <cstdint>
#include <cstdlib>

namespace Zigurat
{

  namespace TLS
  {
  
    typedef uint8_t uint8;
    typedef uint8_t uint16[2];
    typedef uint8_t uint24[3];
    typedef uint8_t uint32[4];
    typedef uint8_t uint64[8];

    typedef uint8_t opaque;

    // The Security Parameters

    enum CompressionMethod : uint8_t { 
      NULL = 0
    };

    enum ConnectionEnd : uint8_t { 
      SERVER, 
      CLIENT 
    };

    enum PRFAlgorithm : uint8_t { 
      TLS_PRF_SHA256 
    };

    enum BulkCipherAlgorithm : uint8_t { 
      NULL, 
      RC4, 
      3DES, 
      AES 
    };

    enum CipherType : uint8_t { 
      STREAM, 
      BLOCK, 
      AEAD
    };

    enum MACAlgorithm : uint8_t { 
      NULL, 
      HMAC_MD5, 
      HMAC_SHA1, 
      HMAC_SHA256, 
      HMAC_SHA384,
      HMAC_SHA512
    };

    struct SecurityParameters {
      ConnectionEnd       entity;
      PRFAlgorithm        prf_algorithm;
      BulkCipherAlgorithm bulk_cipher_algorithm;
      CipherType          cipher_type;
      uint8               enc_key_length;
      uint8               block_length;
      uint8               fixed_iv_length;
      uint8               record_iv_length;
      MACAlgorithm        mac_algorithm;
      uint8               mac_length;
      uint8               mac_key_length;
      CompressionMethod   compression_algorithm;
      opaque              master_secret[48];
      opaque              client_random[32];
      opaque              server_random[32];
    };

    // Record Layer

    struct ProtocolVersion {
      uint8 major;
      uint8 minor;
    } VERSION = {3, 3};                             /* TLS v1.2 */

    enum ContentType : uint8_t {
      CHANGE_CIPHER_SPEC = 20, 
      ALERT              = 21, 
      HANDSHAKE          = 22,
      APPLICATION_DATA   = 23
    };

    struct TLSPlaintext {
      ContentType     type;
      ProtocolVersion version;
      uint16          length;
      opaque*         fragment;                     // [TLSPlaintext.length]
    };

    struct TLSCompressed {
      ContentType     type;
      ProtocolVersion version;
      uint16          length;
      opaque*         fragment;                     // [TLSCompressed.length]
    };

    struct GenericStreamCipher {
      struct {
	opaque* content;                            // [TLSCompressed.length]
	opaque* MAC;                                // [SecurityParameters.mac_length]
      } ciphered;                                   // stream-ciphered
    };

    struct GenericBlockCipher {
      opaque* IV;                                   // [SecurityParameters.record_iv_length]
      struct {
	opaque* content;                            // [TLSCompressed.length]
	opaque* MAC;                                // [SecurityParameters.mac_length]
	uint8   padding;                            // [GenericBlockCipher.padding_length]
	uint8   padding_length;
      } ciphered;                                   // block-ciphered
    };

    struct GenericAEADCipher {
      opaque* nonce_explicit;                       // [SecurityParameters.record_iv_length]
      struct {
	opaque* content;                            // [TLSCompressed.length]
      } ciphered;                                   // aead-ciphered
    };

    union GenericCipher {
      GenericStreamCipher stream;
      GenericBlockCipher  block;
      GenericAEADCipher   aead;
    };

    struct TLSCiphertext {
      ContentType     type;
      ProtocolVersion version;
      uint16          length;
      GenericCipher   fragment;                     // (SecurityParameters.cipher_type)
    };

    // Change Cipher Specs Message

    enum CipherSpecType : uint8_t { 
      CHANGE_CIPHER_SPEC = 1
    };

    struct ChangeCipherSpec {
      CipherSpecType type;
    };

    // Alert Messages

    enum AlertLevel : uint8_t { 
      WARNING = 1, 
      FATAL   = 2 
    };

    enum AlertDescription : uint8_t {
      CLOSE_NOTIFY                = 0,
      UNEXPECTED_MESSAGE          = 10,
      BAD_RECORD_MAC              = 20,
      DECRYPTION_FAILED_RESERVED  = 21,
      RECORD_OVERFLOW             = 22,
      DECOMPRESSION_FAILURE       = 30,
      HANDSHAKE_FAILURE           = 40,
      NO_CERTIFICATE_RESERVED     = 41,
      BAD_CERTIFICATE             = 42,
      UNSUPPORTED_CERTIFICATE     = 43,
      CERTIFICATE_REVOKED         = 44,
      CERTIFICATE_EXPIRED         = 45,
      CERTIFICATE_UNKNOWN         = 46,
      ILLEGAL_PARAMETER           = 47,
      UNKNOWN_CA                  = 48,
      ACCESS_DENIED               = 49,
      DECODE_ERROR                = 50,
      DECRYPT_ERROR               = 51,
      EXPORT_RESTRICTION_RESERVED = 60,
      PROTOCOL_VERSION            = 70,
      INSUFFICIENT_SECURITY       = 71,
      INTERNAL_ERROR              = 80,
      USER_CANCELED               = 90,
      NO_RENEGOTIATION            = 100,
      UNSUPPORTED_EXTENSION       = 110             /* new */
    };

    struct Alert {
      AlertLevel       level;
      AlertDescription description;
    };

    // Hello Messages

    struct HelloRequest { };

    struct Random {
      uint32 gmt_unix_time;
      opaque random_bytes[28];
    };

    typedef opaque SessionID[32];                   // <0..32>

    typedef uint8 CipherSuite[2];

    struct ClientHello {
      ProtocolVersion   client_version;
      Random            random;
      SessionID         session_id;
      CipherSuite       cipher_suites;              // <2..2^16-2>
      CompressionMethod compression_methods;        // <1..2^8-1>
      Extension*        extensions;                 // (extensions_present) <0..2^16-1>
    };

    struct ServerHello {
      ProtocolVersion   server_version;
      Random            random;
      SessionID         session_id;
      CipherSuite       cipher_suite;
      CompressionMethod compression_method;
      Extension*        extensions;                 // (extensions_present) <0..2^16-1>
    };

    struct Extension {
      ExtensionType extension_type;
      opaque*       extension_data;                // <0..2^16-1>
    };

    enum ExtensionType : uint16_t {
      SIGNATURE_ALGORITHMS = 13
    };

    enum HashAlgorithm : uint8_t {
      NONE   = 0, 
      MD5    = 1, 
      SHA1   = 2, 
      SHA224 = 3, 
      SHA256 = 4, 
      SHA384 = 5,
      SHA512 = 6
    };

    enum SignatureAlgorithm : uint8_t {
      ANONYMOUS = 0, 
      RSA       = 1, 
      DSA       = 2, 
      ECDSA     = 3
    };

    struct SignatureAndHashAlgorithm {
      HashAlgorithm      hash;
      SignatureAlgorithm signature;
    } SUPPORTED_SIGNATURE_ALGORITHMS[];             // <2..2^16-1>

    // Server Authentication and Key Exchange Messages

    typedef opaque* ASN_1Cert;                      // <2^24-1>

    struct Certificate {
      ASN_1Cert* certificate_list;                  // <0..2^24-1>
    };

    enum KeyExchangeAlgorithm : uint8_t { 
      DHE_DSS, 
      DHE_RSA, 
      DH_ANON, 
      RSA,DH_DSS, 
      DH_RSA
	                                            /* may be extended, e.g., for ECDH -- see [TLSECC] */
    };

    struct ServerDHParams {                         /* Ephemeral DH parameters */
      opaque* dh_p;                                 // <1..2^16-1>
      opaque* dh_g;                                 // <1..2^16-1>
      opaque* dh_Ys;                                // <1..2^16-1>
    };

    union ServerKeyExchange {                       // (KeyExchangeAlgorithm)
      struct {
	ServerDHParams params;
      } dh_anon;                                    // dh_anon
      struct {
	ServerDHParams params;
	struct {                                    // digitally-signed
	  opaque         client_random[32];
	  opaque         server_random[32];
	  ServerDHParams params;
	} signed_params;      
      } dhe;                                        // dhe_dss, dhe_rsa
      struct { } empty;                             // rsa, dh_dss, dh_rsa
                                                    /* may be extended, e.g., for ECDH -- see [TLSECC] */
    };

    enum ClientCertificateType : uint8_t {
      RSA_SIGN                  = 1, 
	DSS_SIGN                  = 2, 
	RSA_FIXED_DH              = 3, 
	DSS_FIXED_DH              = 4,
	RSA_EPHEMERAL_DH_RESERVED = 5, 
	DSS_EPHEMERAL_DH_RESERVED = 6,
	FORTEZZA_DMS_RESERVED     = 20
	};

    typedef opaque* DistinguishedName;              // <1..2^16-1>

    struct CertificateRequest {
      ClientCertificateType* certificate_types;     // <1..2^8-1>
      DistinguishedName*   certificate_authorities; // <0..2^16-1>
    };

    struct ServerHelloDone { };

    // Client Authentication and Key Exchange Messages

    struct PreMasterSecret {
      ProtocolVersion client_version;
      opaque          random[46];
    };

    struct EncryptedPreMasterSecret {
      PreMasterSecret pre_master_secret;            // public-key-encrypted 
    };

    union ClientKeyExchange {                       // (KeyExchangeAlgorithm)
      struct {                                      // dh_anon, dhe_dss, dhe_rsa, dh_dss, dh_rsa
	ClientDiffieHellmanPublic exchange_keys;
      } dh;
      struct {                                      // rsa
	EncryptedPreMasterSecret exchange_keys;
      } rsa;
    };

    enum PublicValueEncoding : uint8_t { 
      IMPLICIT, 
      EXPLICIT 
    };

    struct ClientDiffieHellmanPublic {              // (PublicValueEncoding)
      union {
	struct { } implicit;                        // implicit
	struct {                                    // explicit
	  opaque* DH_Yc;                            // <1..2^16-1>
	} explicit;
      } dh_public;
    };

    struct CertificateVerify {
      struct {                                      // digitally-signed
	opaque* handshake_messages;                 // [handshake_messages_length]
      };
    };

    // Handshake Finalization Message

    struct Finished {
      opaque* verify_data;                         // [verify_data_length]
    };

    // Handshake Protocol

    enum HandshakeType : uint8_t {
      HELLO_REQUEST       = 0, 
      CLIENT_HELLO        = 1, 
      SERVER_HELLO        = 2,
      CERTIFICATE         = 11, 
      SERVER_KEY_EXCHANGE = 12,
      CERTIFICATE_REQUEST = 13, 
      SERVER_HELLO_DONE   = 14,
      CERTIFICATE_VERIFY  = 15, 
      CLIENT_KEY_EXCHANGE = 16,
      FINISHED            = 20
    };

    struct Handshake {
      HandshakeType msg_type;
      uint24        length;
      union {                                       // (HandshakeType)
	HelloRequest       hello_request;
	ClientHello        client_hello;
	ServerHello        server_hello;
	Certificate        certificate;
	ServerKeyExchange  server_key_exchange;
	CertificateRequest certificate_request;
	ServerHelloDone    server_hello_done;
	CertificateVerify  certificate_verify;
	ClientKeyExchange  client_key_exchange;
	Finished           finished;
      } body;
    };

  }

}

#endif // __TLS_HPP__
