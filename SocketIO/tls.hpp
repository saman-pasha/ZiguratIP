
#ifndef __TLS_HPP__
#define __TLS_HPP__

#include "binarystream.hpp"
#include <vector>
#include <string>

namespace Zigurat
{

  class TLS
  {
  public:
    // The Security Parameters

    enum class ConnectionEnd : uint8_t
    {
      SERVER, 
      CLIENT
    };

    enum class PRFAlgorithm : uint8_t
    { 
      TLS_PRF_SHA256
    };

    enum class BulkCipherAlgorithm : uint8_t
    { 
      NONE,
      RC4,
      DES3,
      AES 
    };

    enum class CipherType : uint8_t
    { 
      STREAM, 
      BLOCK, 
      AEAD
    };

    enum class MACAlgorithm : uint8_t
    { 
      NONE, 
      HMAC_MD5, 
      HMAC_SHA1, 
      HMAC_SHA256, 
      HMAC_SHA384,
      HMAC_SHA512
    };

    enum class CompressionMethod : uint8_t
    { 
      NONE    = 0,
      DEFLATE = 1
    };

    static const size_t MASTER_SECRET_LENGTH = 48;
    static const size_t RANDOM_LENGTH = 32;
    
    struct SecurityParameters
    {
      ConnectionEnd       entity;
      PRFAlgorithm        prf_algorithm;
      BulkCipherAlgorithm bulk_cipher_algorithm;
      CipherType          cipher_type;
      uint8_t             enc_key_length;
      uint8_t             block_length;
      uint8_t             fixed_iv_length;
      uint8_t             record_iv_length;
      MACAlgorithm        mac_algorithm;
      uint8_t             mac_length;
      uint8_t             mac_key_length;
      CompressionMethod   compression_algorithm;
      uint8_t             master_secret[MASTER_SECRET_LENGTH];
      uint8_t             client_random[RANDOM_LENGTH];
      uint8_t             server_random[RANDOM_LENGTH];
    };

    // Record Layer

    enum class ContentType : uint8_t
    {
      CHANGE_CIPHER_SPEC = 20,
      ALERT              = 21,
      HANDSHAKE          = 22,
      APPLICATION_DATA   = 23
    };

    struct ProtocolVersion
    {
      uint8_t major;
      uint8_t minor;
    };

    static const ProtocolVersion VERSION_1_2;       /* TLS v1.2 */

    struct Record                                   // TLSPlaintext
    {
      ContentType     type;
      ProtocolVersion version;
      std::streamsize length;
      binarystream   &fragment;                     // [TLSPlaintext.length]
    };

    // Alert Messages

    enum class AlertLevel : uint8_t
    { 
      WARNING = 1, 
      FATAL   = 2 
    };

    enum class AlertDescription : uint8_t
    {
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

    // Handshake Protocol

    enum class HandshakeType : uint8_t
    {
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

    struct Handshake
    {
      HandshakeType   msg_type;
      std::streamsize length;
      binarystream   &body;                         // [Handshake.length]
    };

    typedef std::vector<uint8_t> SessionID;          // <0..32>

    struct CipherSuite
    {
      uint8_t revision;
      uint8_t suite_id;
    };

    static const CipherSuite TLS_NULL_WITH_NULL_NULL;
    static const CipherSuite TLS_RSA_WITH_NULL_SHA;
    static const CipherSuite TLS_RSA_WITH_NULL_SHA256;
    static const CipherSuite TLS_RSA_WITH_AES_128_CBC_SHA;
    static const CipherSuite TLS_RSA_WITH_AES_256_CBC_SHA;
    static const CipherSuite TLS_RSA_WITH_AES_128_CBC_SHA256;
    static const CipherSuite TLS_RSA_WITH_AES_256_CBC_SHA256;

    // SIGNATURE_ALGORITHMS
    enum class HashAlgorithm : uint8_t
    {
      NONE   = 0, 
      MD5    = 1, 
      SHA1   = 2, 
      SHA224 = 3, 
      SHA256 = 4, 
      SHA384 = 5,
      SHA512 = 6
    };

    enum class SignatureAlgorithm : uint8_t
    {
      ANONYMOUS = 0, 
      RSA       = 1, 
      DSA       = 2, 
      ECDSA     = 3
    };

    struct SignatureAndHashAlgorithm
    {
      HashAlgorithm      hash;
      SignatureAlgorithm signature;
    };

    static const SignatureAndHashAlgorithm SIG_RSA_SHA1;
    static const SignatureAndHashAlgorithm SIG_RSA_SHA224;
    static const SignatureAndHashAlgorithm SIG_RSA_SHA256;
    static const SignatureAndHashAlgorithm SIG_RSA_SHA384;
    static const SignatureAndHashAlgorithm SIG_RSA_SHA512;
    
    enum class ExtensionType : uint16_t
    {
      SIGNATURE_ALGORITHMS = 13,
      RENEGOTIATION_INFO   = 0xFF01      // RFC 5746
    };

    // RFC 5746 signals a peer that understands secure renegotiation, either as
    // the extension above or as this pseudo cipher suite in the offer.
    static const CipherSuite TLS_EMPTY_RENEGOTIATION_INFO_SCSV;

    struct Extension
    {
      ExtensionType extension_type;
      uint8_t      *extension_data;                 // <0..2^16-1>
    };

    // What an end proves itself with, and what it will believe about the other.
    // A ZiguratIP connection is a closed PKI: there is no trust store and no
    // chain to walk, just the one certificate the owner of the server issues to
    // everyone allowed to talk to it. A peer presenting anything that authority
    // did not sign is refused.
    struct Credentials
    {
      std::string certificate;        // this end's own certificate, DER or PEM
      std::string private_key;        // and the key it belongs to
      std::string private_key_cipher; // its pass phrase, empty if there is none
      std::string authority;          // the certificate that must have signed the peer's
    };

    struct HandshakeParameters
    {
      ProtocolVersion                protocol_version;
      SessionID                      session_id;
      std::vector<CipherSuite>       cipher_suites;
      std::vector<CompressionMethod> compression_methods;
      Credentials                    credentials;
    };

    // Change Cipher Specs Message

    enum class CipherSpecType : uint8_t { 
      CHANGE_CIPHER_SPEC = 1
    };

    struct ChangeCipherSpec {
      CipherSpecType type;
    };

    // Hello Messages

    struct HelloRequest { };

    struct Random {
      uint32_t gmt_unix_time;
      uint8_t   random_bytes[28];
    };


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

    // Server Authentication and Key Exchange Messages

    typedef uint8_t* ASN_1Cert;                      // <2^24-1>

    struct Certificate {
      ASN_1Cert* certificate_list;                  // <0..2^24-1>
    };

    enum class KeyExchangeAlgorithm : uint8_t { 
      DHE_DSS, 
      DHE_RSA, 
      DH_ANON, 
      RSA,
      DH_DSS, 
      DH_RSA
	                                            /* may be extended, e.g., for ECDH -- see [TLSECC] */
    };

    struct ServerDHParams {                         /* Ephemeral DH parameters */
      uint8_t* dh_p;                                 // <1..2^16-1>
      uint8_t* dh_g;                                 // <1..2^16-1>
      uint8_t* dh_Ys;                                // <1..2^16-1>
    };

    union ServerKeyExchange {                       // (KeyExchangeAlgorithm)
      struct {
	ServerDHParams params;
      } dh_anon;                                    // dh_anon
      struct {
	ServerDHParams params;
	struct {                                    // digitally-signed
	  uint8_t         client_random[32];
	  uint8_t         server_random[32];
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

    typedef uint8_t* DistinguishedName;              // <1..2^16-1>

    struct CertificateRequest {
      ClientCertificateType* certificate_types;     // <1..2^8-1>
      DistinguishedName*   certificate_authorities; // <0..2^16-1>
    };

    struct ServerHelloDone { };

    // Client Authentication and Key Exchange Messages

    struct PreMasterSecret {
      ProtocolVersion client_version;
      uint8_t          random[46];
    };

    struct EncryptedPreMasterSecret {
      PreMasterSecret pre_master_secret;            // public-key-encrypted 
    };

    struct ClientDiffieHellmanPublic {              // (PublicValueEncoding)
      union {
	struct { } implicit;                        // implicit
	struct {                                    // explicit
	  uint8_t* DH_Yc;                            // <1..2^16-1>
	} explicit_;                                // 'explicit' is a keyword
      } dh_public;
    };

    union ClientKeyExchange {                       // (KeyExchangeAlgorithm)
      struct {                                      // dh_anon, dhe_dss, dhe_rsa, dh_dss, dh_rsa
	ClientDiffieHellmanPublic exchange_keys;
      } dh;
      struct {                                      // rsa
	EncryptedPreMasterSecret exchange_keys;
      } rsa;
    };

    enum class PublicValueEncoding : uint8_t { 
      IMPLICIT, 
      EXPLICIT 
    };

    struct CertificateVerify {
      struct {                                      // digitally-signed
	uint8_t* handshake_messages;                // [handshake_messages_length]
      };
    };

    // Handshake Finalization Message

    struct Finished {
      uint8_t* verify_data;                         // [verify_data_length]
    };

    static void P_SHA256(const uint8_t*, size_t, const uint8_t*, size_t, uint8_t*, size_t);
    static void PRF(PRFAlgorithm, const uint8_t*, size_t, const uint8_t*, size_t, const uint8_t*, size_t, uint8_t*, size_t);
    static void calculate_keys(const SecurityParameters&, uint8_t*, uint8_t*, uint8_t*, uint8_t*, uint8_t*, uint8_t*);
    static void MAC(MACAlgorithm, const uint8_t*, size_t, uint64_t, ContentType, ProtocolVersion, binarystream&, uint16_t, uint8_t*);
    static void IV(uint8_t*, uint8_t);
    static void uint24(uint32_t, binarystream&);
    static uint32_t uint24(binarystream&);
    static void cipher_suite(const CipherSuite&, SecurityParameters&);
    // signature_algorithms, RFC 5246 7.4.1.4.1: the pairs this end will sign
    // and verify with. A TLS 1.2 client that sends none is taken to mean SHA-1
    // only, so it is written whether or not anything else is.
    static void write_signature_algorithms(binarystream&, const std::vector<SignatureAndHashAlgorithm>&);
    static bool accepts_signature_algorithm(binarystream&, std::streamsize, const SignatureAndHashAlgorithm&);
  };

}

#endif // __TLS_HPP__
