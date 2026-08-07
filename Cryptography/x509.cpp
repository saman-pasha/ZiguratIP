#include "x509.hpp"
#include <vector>
#include "cte.hpp"
#include "der.hpp"
#include "rsa.hpp"
#include "aes.hpp"
#include "base64.hpp"
#include "bufferstream.hpp"
#include "configuration.hpp"
#include "utility.hpp"
#include "certificateexception.hpp"
#include <cstring>
#include <memory>
#include <sstream>

// Before namespace Zigurat, because OpenSSL's X509 is a type at global scope
// and this file defines a class of the same name. Inside Zigurat, the OpenSSL
// one has to be written ::X509.
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/asn1.h>


namespace Zigurat
{

  namespace
  {
    // Reading a certificate someone else sent used to mean walking its DER by
    // hand: step over the version, count six fields, find a tag. That code ran
    // on octets from the network, before the peer had proved anything, and it
    // is the last place in this tree that should have been written here rather
    // than taken from a library that many people read. These four functions --
    // subject, public key, permissions, signature -- are the ones a handshake
    // calls on a stranger's certificate, so they go first.

    std::string stream_bytes(binarystream& stream)
    {
      stream.clear();
      stream.seekg(0, std::ios::beg);
      std::string out;
      char buffer[8192];
      while (stream.read(buffer, sizeof(buffer)) || stream.gcount() > 0) {
        out.append(buffer, (size_t)stream.gcount());
        if (!stream) break;
      }
      stream.clear();
      stream.seekg(0, std::ios::beg);
      return out;
    }

    struct BioFree  { void operator()(BIO* b) const { if (b) BIO_free(b); } };
    struct CertFree { void operator()(::X509* c) const { if (c) ::X509_free(c); } };
    struct KeyFree  { void operator()(EVP_PKEY* k) const { if (k) EVP_PKEY_free(k); } };

    // DER first, then PEM. ca writes DER by default and PEM on request, and a
    // peer sends whichever it was given.
    std::unique_ptr< ::X509, CertFree> read_certificate(binarystream& stream)
    {
      const std::string bytes = stream_bytes(stream);
      if (bytes.empty()) throw CertificateException("no certificate");

      std::unique_ptr<BIO, BioFree> bio(BIO_new_mem_buf(bytes.data(), (int)bytes.size()));
      if (!bio) throw CertificateException("out of memory reading a certificate");

      ::X509* cert = d2i_X509_bio(bio.get(), nullptr);
      if (cert == nullptr) {
        BIO_reset(bio.get());
        cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
      }
      if (cert == nullptr) throw CertificateException("not a certificate");
      return std::unique_ptr< ::X509, CertFree>(cert);
    }

    // The short names this tree writes a distinguished name with. Deliberately
    // not X509_NAME_print_ex: its table says GN where this one says givenName,
    // and the rendering is what names the files under home/etc/users, so a
    // different spelling unregisters everyone who is registered. An OID with no
    // entry here is written the way DER::oid_to_string writes one.
    std::string attribute_name(const ASN1_OBJECT* object)
    {
      char dotted[128];
      const int written = OBJ_obj2txt(dotted, sizeof(dotted), object, 1);
      if (written <= 0) return "{ }";
      const std::string oid(dotted, (size_t)((written < (int)sizeof(dotted)) ? written : (int)sizeof(dotted) - 1));

      if (oid == "2.5.4.3")  return "CN";
      if (oid == "2.5.4.4")  return "SN";
      if (oid == "2.5.4.5")  return "serialNumber";
      if (oid == "2.5.4.6")  return "C";
      if (oid == "2.5.4.7")  return "L";
      if (oid == "2.5.4.8")  return "ST";
      if (oid == "2.5.4.10") return "O";
      if (oid == "2.5.4.11") return "OU";
      if (oid == "2.5.4.12") return "title";
      if (oid == "2.5.4.41") return "name";
      if (oid == "2.5.4.42") return "givenName";
      if (oid == "2.5.4.43") return "initials";
      if (oid == "2.5.4.44") return "generationQualifier";
      if (oid == "2.5.4.46") return "dnQualifier";
      if (oid == "2.5.4.65") return "pseudonym";
      if (oid == "0.9.2342.19200300.100.1.25") return "DC";
      if (oid == "1.2.840.113549.1.9.1")       return "emailAddress";

      std::stringstream unknown;          // "{ 2 5 4 99 }", as DER writes it
      unknown << "{ ";
      std::string part;
      std::stringstream in(oid);
      while (std::getline(in, part, '.')) unknown << part << " ";
      unknown << "}";
      return unknown.str();
    }

    // The contents of a DER structure, without its own tag and length. The
    // callers of certificate_public_key wrap what they get in a SEQUENCE
    // themselves, so what comes out here has to be unwrapped the same way it
    // always was.
    std::string der_contents(const unsigned char* der, size_t length)
    {
      if (length < 2) throw CertificateException("truncated DER");
      size_t index = 1;                                  // past the tag
      size_t size = der[index++];
      if (size & 0x80) {
        const size_t count = size & 0x7f;
        if (count == 0 || count > 4 || index + count > length) throw CertificateException("bad DER length");
        size = 0;
        for (size_t i = 0; i < count; i++) size = (size << 8) | der[index++];
      }
      if (index + size > length) throw CertificateException("DER runs past its buffer");
      return std::string((const char*)der + index, size);
    }
  }

  size_t X509::_generate(std::string signature, binarystream& algorithm_id, binarystream& pik, binarystream& puk)
  {
    size_t pos = signature.find('-');
    std::string name((pos == std::string::npos) ? signature : signature.substr(0, pos));
    std::string size((pos == std::string::npos) ? "" : signature.substr(pos + 1));

    if (name == "RSA") {
      if (!(size == "1024" || size == "2048" || size == "3072" || size == "4098")) {
	throw CertificateException("invalid signature algorithm " + signature);
      }

      DER::encode_oid(algorithm_id, {1, 2, 840, 113549, 1, 1, 1});
      DER::encode_null(algorithm_id);

      RSA keygen(std::stoull(size), SHA::SHA1);
      typename RSA::mod_t e = 65537;
      typename RSA::mod_t p, q, dP, dQ, qInv, n, d;
      size_t tries = keygen.RSAKG(e, p, q, dP, dQ, qInv, n, d);

      // PKCS #1 RSAPrivateKey
      bufferstream pik_content;
      DER::encode_integer(pik_content, 0);
      DER::encode_integer(pik_content, n);
      DER::encode_integer(pik_content, e);
      DER::encode_integer(pik_content, d);
      DER::encode_integer(pik_content, p);
      DER::encode_integer(pik_content, q);
      DER::encode_integer(pik_content, dP);
      DER::encode_integer(pik_content, dQ);
      DER::encode_integer(pik_content, qInv);
      DER::encode_sequence(pik, pik_content);

      // PKCS #1 RSAPublicKey
      bufferstream puk_content;
      DER::encode_integer(puk_content, n);
      DER::encode_integer(puk_content, e);
      DER::encode_sequence(puk, puk_content);

      return tries;
    } else {
      throw CertificateException("invalid signature algorithm " + signature);
    }
    return 0;
  }

  void X509::_encrypt(std::string encryption, std::string cipher_key, binarystream& input, binarystream& output, binarystream& algorithm_id)
  {
    size_t pos = encryption.find('-');
    std::string name((pos == std::string::npos) ? encryption : encryption.substr(0, pos));
    std::string size((pos == std::string::npos) ? "" : encryption.substr(pos + 1));
    
    if (name == "AES") {

      uint32_t key_length_id = 0;
      if (size == "128") {
	key_length_id = 2;
	AES128::key_t key;
	cipher_key = Utility::pad_right(cipher_key, 16, '0');
	std::memcpy((uint8_t*)key, cipher_key.c_str(), 16);
	AES128::schedule_t exp_key;
	AES128::KeyExpansion(key, exp_key);
	AES128::Cipher(input, exp_key, output);
      } else if (size == "192") {
	key_length_id = 22;
	AES192::key_t key;
	cipher_key = Utility::pad_right(cipher_key, 24, '0');
	std::memcpy((uint8_t*)key, cipher_key.c_str(), 24);
	AES192::schedule_t exp_key;
	AES192::KeyExpansion(key, exp_key);
	AES192::Cipher(input, exp_key, output);
      } else if (size == "256") {
	key_length_id = 42;
	AES256::key_t key;
	cipher_key = Utility::pad_right(cipher_key, 32, '0');
	std::memcpy((uint8_t*)key, cipher_key.c_str(), 32);
	AES256::schedule_t exp_key;
	AES256::KeyExpansion(key, exp_key);
	AES256::Cipher(input, exp_key, output);
      } else {
	throw CertificateException("invalid encryption algorithm " + encryption);
      }

      DER::encode_oid(algorithm_id, {2, 16, 840, 1, 101, 3, 4, 1, key_length_id});
      DER::encode_null(algorithm_id);

    } else {
      throw CertificateException("invalid encryption algorithm " + encryption);
    }
  }
  
  void X509::_decrypt(binarystream& algorithm_id, std::string cipher_key, binarystream& input, binarystream& output)
  {
    DER::oid_t oid = DER::decode_oid(algorithm_id);
    if (oid[0] == 2 && oid[1] == 16 && oid[2] == 840 && oid[3] == 1 && oid[4] == 101 && oid[5] == 3 && oid[6] == 4 && oid[7] == 1) { // AES
      if (oid[8] == 2) { // 128
	AES128::key_t key;
	cipher_key = Utility::pad_right(cipher_key, 16, '0');
	std::memcpy((uint8_t*)key, cipher_key.c_str(), 16);
	AES128::schedule_t exp_key;
	AES128::KeyExpansion(key, exp_key);
	AES128::InverseCipher(input, exp_key, output);
      } else if (oid[8] == 22) { // 192
	AES192::key_t key;
	cipher_key = Utility::pad_right(cipher_key, 24, '0');
	std::memcpy((uint8_t*)key, cipher_key.c_str(), 24);
	AES192::schedule_t exp_key;
	AES192::KeyExpansion(key, exp_key);
	AES192::InverseCipher(input, exp_key, output);
      } else if (oid[8] == 42) { // 256
	AES256::key_t key;
	cipher_key = Utility::pad_right(cipher_key, 32, '0');
	std::memcpy((uint8_t*)key, cipher_key.c_str(), 32);
	AES256::schedule_t exp_key;
	AES256::KeyExpansion(key, exp_key);
	AES256::InverseCipher(input, exp_key, output);
      } else {
	throw CertificateException("invalid decryption algorithm");
      }      
    } else {
      throw CertificateException("invalid decryption algorithm");
    }
  }

  void X509::_signature_algorithm_id(binarystream& pik_algorithm_id, std::string hash, binarystream& signature_algorithm_id)
  {
    DER::oid_t oid = DER::decode_oid(pik_algorithm_id);

    size_t pos = hash.find('-');
    std::string hash_name((pos == std::string::npos) ? hash : hash.substr(0, pos));
    std::string hash_size((pos == std::string::npos) ? "" : hash.substr(pos + 1));

    if (oid[0] == 1 && oid[1] == 2  && oid[2] == 840 && oid[3] == 113549 && oid[4] == 1 && oid[5] == 1 && oid[6] == 1) { // rsaEncryption
      
      if (hash_name != "SHA") throw CertificateException("invalid hash algorithm " + hash);

      uint32_t hash_id = 0;
      switch (std::stoull(hash_size)) {
      case 1:
	hash_id = 5;    // sha1WithRSAEncryption
	break;
      case 224:
	hash_id = 14;   // sha224WithRSAEncryption
	break;
      case 256:
	hash_id = 11;   // sha256WithRSAEncryption
	break;
      case 384:
	hash_id = 12;   // sha384WithRSAEncryption
	break;
      case 512:
	hash_id = 13;   // sha512WithRSAEncryption
	break;
      default:
        throw CertificateException("invalid hash algorithm " + hash);
      }
      DER::encode_oid(signature_algorithm_id, {1, 2, 840, 113549, 1, 1, hash_id});
      DER::encode_null(signature_algorithm_id);
      
    } else {
      throw CertificateException("invalid signature algorithm " + DER::oid_to_string(oid));
    }      
  }

  void X509::_sign(binarystream& signature_algorithm_id, binarystream& pik, binarystream& message, binarystream& signature)
  {
    DER::oid_t oid = DER::decode_oid(signature_algorithm_id);

    if (oid[0] == 1 && oid[1] == 2  && oid[2] == 840 && oid[3] == 113549 && oid[4] == 1 && oid[5] == 1) { // rsaEncryption

      int version = DER::decode_integer(pik);
      if (version == 0) { // 

	SHA::Version sha_version;
	switch (oid[6]) {
	case 5:    // sha1WithRSAEncryption
	  sha_version = SHA::SHA1;
	  break;
	case 14:   // sha224WithRSAEncryption
	  sha_version = SHA::SHA224;
	  break;
	case 11:   // sha256WithRSAEncryption
	  sha_version = SHA::SHA256;
	  break;
	case 12:   // sha384WithRSAEncryption
	  sha_version = SHA::SHA384;
	  break;
	case 13:   // sha512WithRSAEncryption
	  sha_version = SHA::SHA512;
	  break;
	default:
	  throw CertificateException("invalid signature algorithm " + DER::oid_to_string(oid));
	}

	typename RSA::mod_t n = DER::decode_integer(pik);
	typename RSA::mod_t e = DER::decode_integer(pik);
	typename RSA::mod_t d = DER::decode_integer(pik);

	RSA signer(n.length(false) * 8, sha_version);
	signer.RSASSA_PKCS1_V1_5_Sign(n, d, message, message.length(), signature);

      } else { // Multi-Prime
	throw CertificateException("RSA Multi-Prime key not supported");
      }

    } else {
      throw CertificateException("invalid signature algorithm " + DER::oid_to_string(oid));
    }
  }

  void X509::_verify(binarystream& puk_info, binarystream& signature_algorithm_id, binarystream& message, binarystream& signature)
  {
    DER::oid_t oid = DER::decode_oid(signature_algorithm_id);

    if (oid[0] == 1 && oid[1] == 2  && oid[2] == 840 && oid[3] == 113549 && oid[4] == 1 && oid[5] == 1) {

      SHA::Version sha_version;
      switch (oid[6]) {
      case 5:    // sha1WithRSAEncryption
	sha_version = SHA::SHA1;
	break;
      case 14:   // sha224WithRSAEncryption
	sha_version = SHA::SHA224;
	break;
      case 11:   // sha256WithRSAEncryption
	sha_version = SHA::SHA256;
	break;
      case 12:   // sha384WithRSAEncryption
	sha_version = SHA::SHA384;
	break;
      case 13:   // sha512WithRSAEncryption
	sha_version = SHA::SHA512;
	break;
      default:
	throw CertificateException("invalid signature algorithm " + DER::oid_to_string(oid));
      }

      bufferstream algorithm_id, puk, bit_string;
      DER::decode_sequence(puk_info, algorithm_id);
      DER::decode_bit_string(puk_info, bit_string);
      DER::decode_sequence(bit_string, puk);

      typename RSA::mod_t n = DER::decode_integer(puk);
      typename RSA::mod_t e = DER::decode_integer(puk);

      RSA verifier(n.length(false) * 8, sha_version);
      verifier.RSASSA_PKCS1_V1_5_Verify(n, e, message, message.length(), signature);

    } else {
      throw CertificateException("invalid signature algorithm " + DER::oid_to_string(oid));
    }
  }

  void X509::_load_pik_info(binarystream& pik_stream, std::string cipher_key, binarystream& pik_info, binarystream* puk_info)
  {
    bufferstream line;
    pik_stream.getline(line, 38);
    
    if (line.length() == 37 && line.string() == "-----BEGIN ENCRYPTED PRIVATE KEY-----") {

      line.string("");
      pik_stream.getline(line, '-');
      bufferstream decoded_data;
      CTE::decode(CTE::PEM, line, line.length(), decoded_data);

      bufferstream encrypted_pik_info, encrypted_algorithm_id, encrypted_data;
      DER::decode_sequence(decoded_data, encrypted_pik_info);
      DER::decode_sequence(encrypted_pik_info, encrypted_algorithm_id);
      DER::decode_octet_string(encrypted_pik_info, encrypted_data);

      line.string("");
      X509::_decrypt(encrypted_algorithm_id, cipher_key, encrypted_data, line);
      DER::decode_sequence(line, pik_info);

    } else if (line.length() == 27 && line.string() == "-----BEGIN PRIVATE KEY-----") {

      line.string("");
      pik_stream.getline(line, '-');
      bufferstream decoded_data;
      CTE::decode(CTE::PEM, line, line.length(), decoded_data);
      DER::decode_sequence(decoded_data, pik_info);

    } else {

      uint8_t tag = DER::decode_tag(line);
      if (tag != DER::SEQUENCE) throw CertificateException("invalid private key");
      size_t length = DER::decode_length(line);
      if (length == 0) throw CertificateException("invalid private key");
      tag = DER::decode_tag(line);
      
      pik_stream.clear();
      pik_stream.seekg(0, std::ios::beg);

      if (tag == DER::INTEGER) {

	DER::decode_sequence(pik_stream, pik_info);	

      } else {

	bufferstream encrypted_pik_info, encrypted_algorithm_id, encrypted_data;
	DER::decode_sequence(pik_stream, encrypted_pik_info);
	DER::decode_sequence(encrypted_pik_info, encrypted_algorithm_id);
	DER::decode_octet_string(encrypted_pik_info, encrypted_data);
	
	line.string("");
	X509::_decrypt(encrypted_algorithm_id, cipher_key, encrypted_data, line);
	DER::decode_sequence(line, pik_info);

      }
    }

    if (puk_info != nullptr) {

      bufferstream tmp_pik_info;
      pik_info.read(tmp_pik_info, 0, pik_info.length());

      bufferstream pik_algorithm_id, pik;
      X509::_extract_pik_info(tmp_pik_info, pik_algorithm_id, pik);

      DER::oid_t oid = DER::decode_oid(pik_algorithm_id);
      if (oid[0] == 1 && oid[1] == 2  && oid[2] == 840 && oid[3] == 113549 && oid[4] == 1 && oid[5] == 1 && oid[6] == 1) {

	int version = DER::decode_integer(pik);
	if (version == 0) {

	  typename RSA::mod_t n = DER::decode_integer(pik);
	  typename RSA::mod_t e = DER::decode_integer(pik);
	  
	  // PKCS #1 RSAPublicKey
	  bufferstream puk, puk_content;
	  DER::encode_integer(puk_content, n);
	  DER::encode_integer(puk_content, e);
	  DER::encode_sequence(puk, puk_content);
	  
	  // X.509 v3 PublicKeyInfo
	  DER::encode_sequence(*puk_info, pik_algorithm_id);
	  DER::encode_bit_string(*puk_info, puk, puk.length() * 8);
	  
	} else {
	  throw CertificateException("invalid signature algorithm " + DER::oid_to_string(oid)); // Multi-Prime
	}
      } else {
	throw CertificateException("invalid signature algorithm " + DER::oid_to_string(oid));
      }
    }

    // Deriving the public key above read pik_info to the end. Callers parse it
    // again straight afterwards, so hand it back rewound rather than at eof.
    pik_info.clear();
    pik_info.seekg(0, std::ios::beg);
  }

  void X509::_load_puk_info(binarystream& puk_stream, binarystream& puk_info)
  {
    bufferstream line;
    puk_stream.getline(line, 27);

    if (line.length() == 26 && line.string() == "-----BEGIN PUBLIC KEY-----") {

      line.string("");
      puk_stream.getline(line, '-');
      bufferstream decoded_data;
      CTE::decode(CTE::PEM, line, line.length(), decoded_data);
      DER::decode_sequence(decoded_data, puk_info);

    } else {

      puk_stream.clear();
      puk_stream.seekg(0, std::ios::beg);
      DER::decode_sequence(puk_stream, puk_info);	

    }
  }

  void X509::_load_csr(binarystream& csr_stream, binarystream& csr)
  {
    bufferstream line;
    csr_stream.getline(line, 36);

    if (line.length() == 35 && line.string() == "-----BEGIN CERTIFICATE REQUEST-----") {

      line.string("");
      csr_stream.getline(line, '-');
      bufferstream decoded_data;
      CTE::decode(CTE::PEM, line, line.length(), decoded_data);
      DER::decode_sequence(decoded_data, csr);

    } else {

      csr_stream.clear();
      csr_stream.seekg(0, std::ios::beg);
      DER::decode_sequence(csr_stream, csr);	

    }
  }

  void X509::_load_certificate(binarystream& crt_stream, binarystream& certificate)
  {
    bufferstream line;
    crt_stream.getline(line, 28);

    if (line.length() == 27 && line.string() == "-----BEGIN CERTIFICATE-----") {

      line.string("");
      crt_stream.getline(line, '-');
      bufferstream decoded_data;
      CTE::decode(CTE::PEM, line, line.length(), decoded_data);
      DER::decode_sequence(decoded_data, certificate);

    } else {

      crt_stream.clear();
      crt_stream.seekg(0, std::ios::beg);
      DER::decode_sequence(crt_stream, certificate);	

    }
  }

  void X509::_dump_pik_info(binarystream& pik_info, std::string encryption, std::string cipher_key, std::string encoding, binarystream& pik_stream)
  {
    if (encryption.size() > 0) {
 
      bufferstream pure_data, encrypted_data, encryption_algorithm_id;
      DER::encode_sequence(pure_data, pik_info);
      X509::_encrypt(encryption, cipher_key, pure_data, encrypted_data, encryption_algorithm_id);

      // PKCS #8 EncryptedPrivateKeyInfo
      bufferstream encrypted_pik_info;
      DER::encode_sequence(encrypted_pik_info, encryption_algorithm_id);
      DER::encode_octet_string(encrypted_pik_info, encrypted_data);
  
      if (encoding == "DER") {
	DER::encode_sequence(pik_stream, encrypted_pik_info);
      } else if (encoding == "PEM") {
	pik_stream.write("-----BEGIN ENCRYPTED PRIVATE KEY-----\n", 38);
	bufferstream pure_data, encoded_data;
	DER::encode_sequence(pure_data, encrypted_pik_info);
	CTE::encode(CTE::PEM, pure_data, pure_data.length(), encoded_data);
	encoded_data.read(pik_stream, 0, encoded_data.length());
	pik_stream.write("\n-----END ENCRYPTED PRIVATE KEY-----", 36);
      } else {
	throw CertificateException("invalid encoding algorithm " + encoding);
      }

    } else {

      if (encoding == "DER") {
	DER::encode_sequence(pik_stream, pik_info);
      } else if (encoding == "PEM") {
	pik_stream.write("-----BEGIN PRIVATE KEY-----\n", 28);
	bufferstream pure_data, encoded_data;
	DER::encode_sequence(pure_data, pik_info);
	CTE::encode(CTE::PEM, pure_data, pure_data.length(), encoded_data);
	encoded_data.read(pik_stream, 0, encoded_data.length());
	pik_stream.write("\n-----END PRIVATE KEY-----", 26);
      } else {
	throw CertificateException("invalid encoding algorithm " + encoding);
      }

    }
  }

  void X509::_dump_puk_info(binarystream& puk_info, std::string encoding, binarystream& puk_stream)
  {
    if (encoding == "DER") {
      DER::encode_sequence(puk_stream, puk_info);
    } else if (encoding == "PEM") {
      puk_stream.write("-----BEGIN PUBLIC KEY-----\n", 27);
      bufferstream pure_data, encoded_data;
      DER::encode_sequence(pure_data, puk_info);
      CTE::encode(CTE::PEM, pure_data, pure_data.length(), encoded_data);
      encoded_data.read(puk_stream, 0, encoded_data.length());
      puk_stream.write("\n-----END PUBLIC KEY-----", 25);
    } else {
      throw CertificateException("invalid encoding algorithm " + encoding);
    }
  }

  void X509::_dump_csr(binarystream& csr, std::string encoding, binarystream& csr_stream)
  {
    if (encoding == "DER") {
      DER::encode_sequence(csr_stream, csr);
    } else if (encoding == "PEM") {
      csr_stream.write("-----BEGIN CERTIFICATE REQUEST-----\n", 36);
      bufferstream pure_data, encoded_data;
      DER::encode_sequence(pure_data, csr);
      CTE::encode(CTE::PEM, pure_data, pure_data.length(), encoded_data);
      encoded_data.read(csr_stream, 0, encoded_data.length());
      csr_stream.write("\n-----END CERTIFICATE REQUEST-----", 34);
    } else {
      throw CertificateException("invalid encoding algorithm " + encoding);
    }
  }

  void X509::_dump_certificate(binarystream& certificate, std::string encoding, binarystream& crt_stream)
  {
    if (encoding == "DER") {
      DER::encode_sequence(crt_stream, certificate);
    } else if (encoding == "PEM") {
      crt_stream.write("-----BEGIN CERTIFICATE-----\n", 28);
      bufferstream pure_data, encoded_data;
      DER::encode_sequence(pure_data, certificate);
      CTE::encode(CTE::PEM, pure_data, pure_data.length(), encoded_data);
      encoded_data.read(crt_stream, 0, encoded_data.length());
      crt_stream.write("\n-----END CERTIFICATE-----", 26);
    } else {
      throw CertificateException("invalid encoding algorithm " + encoding);
    }
  }

  void X509::_extract_pik_info(binarystream& pik_info, binarystream& pik_algorithm_id, binarystream& pik)
  {
    bufferstream pik_data;
    DER::decode_integer(pik_info);    // Version
    DER::decode_sequence(pik_info, pik_algorithm_id);
    DER::decode_octet_string(pik_info, pik_data);
    DER::decode_sequence(pik_data, pik);
  }

  void X509::_attribute(binarystream& rdns, std::string attr_name, std::string attr_value)
  {
    bufferstream attribute_set, attribute;

    std::string value = Utility::trim(attr_value, ' ');
    if (value.size() == 0) return;

    if (attr_name == "COUNTRY") {
      if (value.size() != 2) throw CertificateException("invalid issuer country " + value);
      DER::encode_oid(attribute, {2, 5, 4, 6});
      DER::encode_printable_string(attribute, value);
    } else if (attr_name == "ORGANIZATION") {
      DER::encode_oid(attribute, {2, 5, 4, 10});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "ORGANIZATIONAL_UNIT") {
      DER::encode_oid(attribute, {2, 5, 4, 11});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "DISTINGUISHED_NAME_QUALIFIER") {
      DER::encode_oid(attribute, {2, 5, 4, 46});
      DER::encode_printable_string(attribute, value);
    } else if (attr_name == "STATE_OR_PROVINCE_NAME") {
      DER::encode_oid(attribute, {2, 5, 4, 8});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "COMMON_NAME") {
      DER::encode_oid(attribute, {2, 5, 4, 3});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "SERIAL_NUMBER") {
      DER::encode_oid(attribute, {2, 5, 4, 5});
      DER::encode_printable_string(attribute, value);
    } else if (attr_name == "LOCALITY") {
      DER::encode_oid(attribute, {2, 5, 4, 7});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "TITLE") {
      DER::encode_oid(attribute, {2, 5, 4, 12});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "NAME") {
      DER::encode_oid(attribute, {2, 5, 4, 41});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "SURNAME") {
      DER::encode_oid(attribute, {2, 5, 4, 4});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "GIVEN_NAME") {
      DER::encode_oid(attribute, {2, 5, 4, 42});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "INITIALS") {
      DER::encode_oid(attribute, {2, 5, 4, 43});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "PSEUDONYM") {
      DER::encode_oid(attribute, {2, 5, 4, 65});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "GENERATION_QUALIFIER") {
      DER::encode_oid(attribute, {2, 5, 4, 44});
      DER::encode_utf8_string(attribute, value);
    } else if (attr_name == "DOMAIN_COMPONENT") {
      DER::encode_oid(attribute, {0, 9, 2342, 19200300, 100, 1, 25});
      DER::encode_ia5_string(attribute, value);
    } else if (attr_name == "EMAIL_ADDRESS") {
      DER::encode_oid(attribute, {1, 2, 840, 113549, 1, 9, 1});
      DER::encode_ia5_string(attribute, value);
    } else {
      throw CertificateException("invalid issuer relative distinguished name " + attr_name);
    }

    DER::encode_sequence(attribute_set, attribute);
    if (attribute_set.length() > 0) DER::encode_set(rdns, attribute_set);
  }

  void X509::_name(binarystream& stream, binarystream& rdns)
  {
    Configuration config(stream);
    std::string attr_value;

    if (config.get("/COUNTRY", attr_value)) {
      X509::_attribute(rdns, "COUNTRY", attr_value);
    }
    if (config.get("/ORGANIZATION", attr_value)) {
      X509::_attribute(rdns, "ORGANIZATION", attr_value);
    }
    if (config.get("/ORGANIZATIONAL_UNIT", attr_value)) {
      X509::_attribute(rdns, "ORGANIZATIONAL_UNIT", attr_value);
    }
    if (config.get("/DISTINGUISHED_NAME_QUALIFIER", attr_value)) {
      X509::_attribute(rdns, "DISTINGUISHED_NAME_QUALIFIER", attr_value);
    }
    if (config.get("/STATE_OR_PROVINCE_NAME", attr_value)) {
      X509::_attribute(rdns, "STATE_OR_PROVINCE_NAME", attr_value);
    }
    if (config.get("/COMMON_NAME", attr_value)) {
      X509::_attribute(rdns, "COMMON_NAME", attr_value);
    }
    if (config.get("/SERIAL_NUMBER", attr_value)) {
      X509::_attribute(rdns, "SERIAL_NUMBER", attr_value);
    }
    if (config.get("/LOCALITY", attr_value)) {
      X509::_attribute(rdns, "LOCALITY", attr_value);
    }
    if (config.get("/TITLE", attr_value)) {
      X509::_attribute(rdns, "TITLE", attr_value);
    }
    if (config.get("/NAME", attr_value)) {
      X509::_attribute(rdns, "NAME", attr_value);
    }
    if (config.get("/SURNAME", attr_value)) {
      X509::_attribute(rdns, "SURNAME", attr_value);
    }
    if (config.get("/GIVEN_NAME", attr_value)) {
      X509::_attribute(rdns, "GIVEN_NAME", attr_value);
    }
    if (config.get("/INITIALS", attr_value)) {
      X509::_attribute(rdns, "INITIALS", attr_value);
    }
    if (config.get("/PSEUDONYM", attr_value)) {
      X509::_attribute(rdns, "PSEUDONYM", attr_value);
    }
    if (config.get("/GENERATION_QUALIFIER", attr_value)) {
      X509::_attribute(rdns, "GENERATION_QUALIFIER", attr_value);
    }
    if (config.get("/DOMAIN_COMPONENT", attr_value)) {
      X509::_attribute(rdns, "DOMAIN_COMPONENT", attr_value);
    }
    if (config.get("/EMAIL_ADDRESS", attr_value)) {
      X509::_attribute(rdns, "EMAIL_ADDRESS", attr_value);
    }
  }

  size_t X509::keygen(std::string signature, std::string encryption, std::string cipher_key, std::string encoding, 
		      binarystream& pik_stream, binarystream& puk_stream)
  {
    signature  = Utility::to_upper(signature);
    encryption = Utility::to_upper(encryption);
    encoding   = Utility::to_upper(encoding);

    bufferstream algorithm_id, pik, puk, pik_info, puk_info;

    size_t tries = X509::_generate(signature, algorithm_id, pik, puk);

    // PKCS #8 PrivateKeyInfo
    DER::encode_integer(pik_info, 0);
    DER::encode_sequence(pik_info, algorithm_id);
    DER::encode_octet_string(pik_info, pik);

    // X.509 v3 PublicKeyInfo
    DER::encode_sequence(puk_info, algorithm_id);
    DER::encode_bit_string(puk_info, puk, puk.length() * 8);

    X509::_dump_pik_info(pik_info, encryption, cipher_key, encoding, pik_stream);
    X509::_dump_puk_info(puk_info, encoding, puk_stream);

    return tries;
  }

  void X509::csr(binarystream& subject_stream, binarystream& pik_stream, std::string cipher_key, 
		 std::string hash, std::string encoding, binarystream& csr_stream)
  {
    hash     = Utility::to_upper(hash);
    encoding = Utility::to_upper(encoding);

    // PKCS #10 CertificationRequestInfo
    bufferstream csr_info;
    DER::encode_integer(csr_info, 0);

    bufferstream subject;
    X509::_name(subject_stream, subject);
    DER::encode_sequence(csr_info, subject);
    
    bufferstream pik_info, puk_info, pik_algorithm_id, pik;
    X509::_load_pik_info(pik_stream, cipher_key, pik_info, &puk_info);

    DER::encode_sequence(csr_info, puk_info);

    // PKCS #10 CertificationRequest
    bufferstream csr;
    DER::encode_sequence(csr, csr_info);

    bufferstream signature_algorithm_id;
    X509::_extract_pik_info(pik_info, pik_algorithm_id, pik);
    X509::_signature_algorithm_id(pik_algorithm_id, hash, signature_algorithm_id);
    DER::encode_sequence(csr, signature_algorithm_id);

    bufferstream csr_info_data, signature_value;
    DER::encode_sequence(csr_info_data, csr_info);
    X509::_sign(signature_algorithm_id, pik, csr_info_data, signature_value);
    DER::encode_bit_string(csr, signature_value, signature_value.length() * 8);

    X509::_dump_csr(csr, encoding, csr_stream);
  }

  const char* X509::PERMISSIONS_OID = "1.3.6.1.4.1.55447.1.1";

  // The same arc as nodes, which is what the encoder wants.
  static DER::oid_t permissions_oid()
  {
    return DER::oid_t {1, 3, 6, 1, 4, 1, 55447, 1, 1};
  }

  void X509::issue(binarystream& serial_number, binarystream& issuer_stream, binarystream& pik_stream, std::string cipher_key,
		   time_t not_before_time, time_t not_after_time, binarystream& csr_stream,
		   std::string hash, std::string encoding, binarystream& crt_stream)
  {
    X509::issue(serial_number, issuer_stream, pik_stream, cipher_key, not_before_time, not_after_time,
		csr_stream, hash, encoding, std::vector<std::string>(), crt_stream);
  }

  void X509::issue(binarystream& serial_number, binarystream& issuer_stream, binarystream& pik_stream, std::string cipher_key,
		   time_t not_before_time, time_t not_after_time, binarystream& csr_stream,
		   std::string hash, std::string encoding, const std::vector<std::string>& permissions,
		   binarystream& crt_stream)
  {
    hash     = Utility::to_upper(hash);
    encoding = Utility::to_upper(encoding);

    // A certificate is v1 unless it carries something only v3 can hold. Version
    // is [0] EXPLICIT and defaults to v1 when absent, so emitting it only when
    // it is needed keeps every certificate issued so far byte for byte the same.
    const bool is_v3 = !permissions.empty();

    bufferstream tbs_certificate;

    if (is_v3) {
      bufferstream version;
      DER::encode_integer(version, 2);                   // v3
      DER::encode_context(tbs_certificate, version, 0);
    }

    DER::encode_integer(tbs_certificate, serial_number);

    bufferstream pik_info, pik_algorithm_id, signature_algorithm_id, pik;
    X509::_load_pik_info(pik_stream, cipher_key, pik_info);
    X509::_extract_pik_info(pik_info, pik_algorithm_id, pik);
    X509::_signature_algorithm_id(pik_algorithm_id, hash, signature_algorithm_id);
    DER::encode_sequence(tbs_certificate, signature_algorithm_id);

    bufferstream issuer;
    X509::_name(issuer_stream, issuer);
    DER::encode_sequence(tbs_certificate, issuer);

    bufferstream validity;
    DER::encode_generalized_time(validity, not_before_time);
    DER::encode_generalized_time(validity, not_after_time);
    DER::encode_sequence(tbs_certificate, validity);

    bufferstream csr, csr_info_data, csr_info, csr_signature_algorithm_id, csr_signature_value;
    X509::_load_csr(csr_stream, csr);
    DER::decode_tlv(csr, csr_info_data);
    DER::decode_sequence(csr, csr_signature_algorithm_id);
    DER::decode_bit_string(csr, csr_signature_value);
    DER::decode_sequence(csr_info_data, csr_info);

    bufferstream subject, puk_info;
    DER::decode_integer(csr_info);
    DER::decode_sequence(csr_info, subject);
    DER::decode_sequence(csr_info, puk_info);
    csr_info_data.clear();
    csr_info_data.seekg(0, std::ios::beg);
    X509::_verify(puk_info, csr_signature_algorithm_id, csr_info_data, csr_signature_value);

    DER::encode_sequence(tbs_certificate, subject);
    DER::encode_sequence(tbs_certificate, puk_info);

    // Extensions: [3] EXPLICIT SEQUENCE OF Extension, and one Extension is
    // { OID, critical BOOLEAN DEFAULT FALSE, OCTET STRING }. The octet string
    // holds the DER of whatever the extension means -- here a SEQUENCE OF
    // UTF8String, one per permission.
    //
    // Not marked critical. A certificate that says what its holder may do
    // should still be readable by something that has never heard of these
    // permissions; marking it critical would make every other implementation
    // reject the certificate outright.
    if (is_v3) {
      bufferstream granted, extension, extensions, extensions_wrapper;

      for (const std::string& permission : permissions)
	DER::encode_utf8_string(granted, permission);

      bufferstream payload, payload_octets;
      DER::encode_sequence(payload, granted);
      DER::encode_octet_string(payload_octets, payload);

      DER::encode_oid(extension, permissions_oid());
      extension.write(payload_octets, payload_octets.length());

      DER::encode_sequence(extensions, extension);
      DER::encode_sequence(extensions_wrapper, extensions);
      DER::encode_context(tbs_certificate, extensions_wrapper, 3);
    }

    // X.509 v3 Certificate
    bufferstream certificate, tbs_certificate_data, signature_value;
    DER::encode_sequence(certificate, tbs_certificate);
    DER::encode_sequence(certificate, signature_algorithm_id);
    DER::encode_sequence(tbs_certificate_data, tbs_certificate);
    X509::_sign(signature_algorithm_id, pik, tbs_certificate_data, signature_value);
    DER::encode_bit_string(certificate, signature_value, signature_value.length() * 8);

    X509::_dump_certificate(certificate, encoding, crt_stream);
  }

  void X509::validate_by_pik(binarystream& pik_stream, std::string cipher_key, binarystream& crt_stream)
  {
    bufferstream pik_info, puk_info;
    X509::_load_pik_info(pik_stream, cipher_key, pik_info, &puk_info);

    bufferstream certificate;
    X509::_load_certificate(crt_stream, certificate);

    bufferstream tbs_certificate_data, signature_algorithm_id, signature_value;
    DER::decode_tlv(certificate, tbs_certificate_data);
    DER::decode_sequence(certificate, signature_algorithm_id);
    DER::decode_bit_string(certificate, signature_value);

    X509::_verify(puk_info, signature_algorithm_id, tbs_certificate_data, signature_value);
  }

  // tbsCertificate is
  //
  //     serialNumber, signature, issuer, validity, subject, subjectPublicKeyInfo
  //
  // for the v1 certificates the CA issues, so the key is the sixth element and
  // the subject the fifth. Nothing here assumes extensions; when they arrive the
  // optional [0] version has to be stepped over first.
  static void _tbs_element(binarystream& certificate, int index, binarystream& element)
  {
    bufferstream tbs;
    DER::decode_sequence(certificate, tbs);

    // Version is [0] EXPLICIT and only present on a v3 certificate, where it
    // sits in front of the serial number. Everything below counts from the
    // serial, so it is stepped over when it is there and nothing changes when
    // it is not.
    bufferstream version;
    DER::decode_context(tbs, version, 0);

    // Every step is checked against what is left. The bytes come off the wire --
    // a peer presents its own certificate -- so a structure that is not a
    // certificate at all, or is truncated, has to be refused rather than walked
    // off the end of. Handing this a public key file used to reach past the
    // buffer and take the process down with a bus error.
    for (int i = 0; i <= index; i++) {

      if (tbs.tellg() >= tbs.length())
	throw CertificateException("certificate ends before element "
				   + std::to_string(index) + " of tbsCertificate");

      bufferstream skipped;
      DER::decode_tlv(tbs, (i == index) ? element : skipped);
    }

    if (element.length() == 0)
      throw CertificateException("empty element " + std::to_string(index) + " of tbsCertificate");
  }

  std::vector<std::string> X509::certificate_permissions(binarystream& crt_stream)
  {
    std::vector<std::string> permissions;

    auto certificate = read_certificate(crt_stream);

    std::unique_ptr<ASN1_OBJECT, void(*)(ASN1_OBJECT*)>
      oid(::OBJ_txt2obj(X509::PERMISSIONS_OID, 1), ::ASN1_OBJECT_free);
    if (!oid) throw CertificateException("cannot read the permissions oid");

    const int where = ::X509_get_ext_by_OBJ(certificate.get(), oid.get(), -1);
    if (where < 0) return permissions;    // v1, and anything issued before this existed

    ::X509_EXTENSION* extension = ::X509_get_ext(certificate.get(), where);
    if (extension == nullptr) return permissions;

    const ASN1_OCTET_STRING* payload = ::X509_EXTENSION_get_data(extension);
    if (payload == nullptr) return permissions;

    // Inside the octet string is a SEQUENCE OF UTF8String, which is the shape
    // issue() writes. Walking it is left to the ASN.1 decoder rather than done
    // by hand, since these octets came off the wire.
    const unsigned char* p = ::ASN1_STRING_get0_data(payload);
    long remaining = ::ASN1_STRING_length(payload);

    long body = 0;
    int tag = 0, cls = 0;
    if (::ASN1_get_object(&p, &body, &tag, &cls, remaining) & 0x80)
      throw CertificateException("permissions extension is malformed");
    if (tag != V_ASN1_SEQUENCE) throw CertificateException("permissions extension is not a sequence");

    const unsigned char* end = p + body;
    while (p < end) {
      long item = 0;
      if (::ASN1_get_object(&p, &item, &tag, &cls, end - p) & 0x80)
        throw CertificateException("permissions extension is malformed");
      if (tag != V_ASN1_UTF8STRING) throw CertificateException("a permission is not a utf8 string");
      permissions.push_back(std::string((const char*)p, (size_t)item));
      p += item;
    }

    return permissions;
  }

  void X509::certificate_public_key(binarystream& crt_stream, binarystream& puk_info)
  {
    auto certificate = read_certificate(crt_stream);

    ::X509_PUBKEY* pubkey = ::X509_get_X509_PUBKEY(certificate.get());
    if (pubkey == nullptr) throw CertificateException("certificate carries no public key");

    unsigned char* der = nullptr;
    const int length = ::i2d_X509_PUBKEY(pubkey, &der);
    if (length <= 0 || der == nullptr) throw CertificateException("cannot encode the public key");

    std::string contents;
    try {
      // i2d gives a whole SubjectPublicKeyInfo; this has always handed back its
      // contents and left the caller to wrap them, and tlsbuf and ca both do.
      contents = der_contents(der, (size_t)length);
    } catch (...) {
      OPENSSL_free(der);
      throw;
    }
    OPENSSL_free(der);

    puk_info.write(contents.data(), (std::streamsize)contents.size());
    puk_info.seekg(0, std::ios::beg);
  }

  // Rendered the way OpenSSL prints a name -- "C=US, CN=..." -- so what a
  // permission would be keyed on reads the same in both tools.
  static std::string _attribute_name(DER::oid_t& oid)
  {
    if (oid.size() == 4 && oid[0] == 2 && oid[1] == 5 && oid[2] == 4) {
      switch (oid[3]) {
      case  3: return "CN";
      case  4: return "SN";
      case  5: return "serialNumber";
      case  6: return "C";
      case  7: return "L";
      case  8: return "ST";
      case 10: return "O";
      case 11: return "OU";
      case 12: return "title";
      case 41: return "name";
      case 42: return "givenName";
      case 43: return "initials";
      case 44: return "generationQualifier";
      case 46: return "dnQualifier";
      case 65: return "pseudonym";
      default: break;
      }
    }
    if (oid.size() == 7 && oid[0] == 0 && oid[1] == 9 && oid[2] == 2342
	&& oid[3] == 19200300 && oid[4] == 100 && oid[5] == 1 && oid[6] == 25)
      return "DC";
    if (oid.size() == 7 && oid[0] == 1 && oid[1] == 2 && oid[2] == 840
	&& oid[3] == 113549 && oid[4] == 1 && oid[5] == 9 && oid[6] == 1)
      return "emailAddress";

    return DER::oid_to_string(oid);
  }

  std::string X509::certificate_subject(binarystream& crt_stream)
  {
    auto certificate = read_certificate(crt_stream);
    ::X509_NAME* subject = ::X509_get_subject_name(certificate.get());
    if (subject == nullptr) throw CertificateException("certificate has no subject");

    std::string name;
    const int count = ::X509_NAME_entry_count(subject);
    for (int i = 0; i < count; i++) {
      ::X509_NAME_ENTRY* entry = ::X509_NAME_get_entry(subject, i);
      if (entry == nullptr) continue;

      const ASN1_OBJECT* object = ::X509_NAME_ENTRY_get_object(entry);
      const ASN1_STRING* value  = ::X509_NAME_ENTRY_get_data(entry);
      if (object == nullptr || value == nullptr) continue;

      // The octets as they stand, which is what the hand written reader took:
      // no transcoding, so a UTF-8 name stays the bytes it arrived as.
      const std::string text((const char*)::ASN1_STRING_get0_data(value),
                             (size_t)::ASN1_STRING_length(value));

      if (!name.empty()) name += ", ";
      name += attribute_name(object) + "=" + text;
    }

    return name;
  }

  std::string X509::subject_file_name(const std::string& subject)
  {
    static const char hex[] = "0123456789ABCDEF";

    std::string encoded;
    for (unsigned char c : subject) {
      const bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
	|| (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '=';
      if (plain) {
	encoded.push_back((char)c);
      } else {
	encoded.push_back('%');
	encoded.push_back(hex[c >> 4]);
	encoded.push_back(hex[c & 0x0F]);
      }
    }

    return encoded;
  }

  std::string X509::file_name_subject(const std::string& file_name)
  {
    auto value([] (char c) -> int {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	return -1;
      });

    std::string subject;
    for (size_t i = 0; i < file_name.size(); i++) {
      if (file_name[i] != '%' || i + 2 >= file_name.size()) {
	subject.push_back(file_name[i]);
	continue;
      }

      const int high = value(file_name[i + 1]), low = value(file_name[i + 2]);
      if (high < 0 || low < 0) {
	// Not an escape after all, whatever it looked like. Left as written
	// rather than guessed at, so a hand made file name still round trips.
	subject.push_back(file_name[i]);
	continue;
      }

      subject.push_back((char)((high << 4) | low));
      i += 2;
    }

    return subject;
  }

  void X509::sign(binarystream& pik_stream, std::string cipher_key, std::string hash,
		  binarystream& message, binarystream& signature)
  {
    hash = Utility::to_upper(hash);

    bufferstream pik_info, pik_algorithm_id, pik;
    X509::_load_pik_info(pik_stream, cipher_key, pik_info);
    X509::_extract_pik_info(pik_info, pik_algorithm_id, pik);

    bufferstream signature_algorithm_id;
    X509::_signature_algorithm_id(pik_algorithm_id, hash, signature_algorithm_id);

    X509::_sign(signature_algorithm_id, pik, message, signature);
  }

  bool X509::verify(binarystream& crt_stream, std::string hash,
		    binarystream& message, binarystream& signature)
  {
    hash = Utility::to_upper(hash);

    bufferstream puk_info;
    X509::certificate_public_key(crt_stream, puk_info);

    // _signature_algorithm_id wants the key's own algorithm id to decide what it
    // may be signed with, and the certificate carries it in front of the key.
    bufferstream puk_copy, algorithm_id;
    puk_info.read(puk_copy, 0, puk_info.length());
    DER::decode_sequence(puk_copy, algorithm_id);

    bufferstream signature_algorithm_id;
    X509::_signature_algorithm_id(algorithm_id, hash, signature_algorithm_id);

    try {
      X509::_verify(puk_info, signature_algorithm_id, message, signature);
    } catch (const std::exception&) {
      return false;
    }
    return true;
  }

  void X509::encrypt(binarystream& crt_stream, binarystream& plain, binarystream& cipher)
  {
    bufferstream puk_info;
    X509::certificate_public_key(crt_stream, puk_info);

    bufferstream algorithm_id, bit_string, puk;
    DER::decode_sequence(puk_info, algorithm_id);
    DER::decode_bit_string(puk_info, bit_string);
    DER::decode_sequence(bit_string, puk);

    typename RSA::mod_t n = DER::decode_integer(puk);
    typename RSA::mod_t e = DER::decode_integer(puk);

    const size_t k = (size_t)n.length(false);
    RSA engine(k * 8, SHA::SHA256);

    const std::streamsize length = plain.length();
    std::vector<uint8_t> message((size_t)(length > 0 ? length : 1));
    plain.read((char*)message.data(), 0, length);

    std::vector<uint8_t> encrypted(k);
    engine.RSAES_PKCS1_V1_5_Encrypt(n, e, message.data(), (int64_t)length, encrypted.data());

    cipher.write((char*)encrypted.data(), (std::streamsize)k);
  }

  void X509::decrypt(binarystream& pik_stream, std::string cipher_key,
		     binarystream& cipher, binarystream& plain)
  {
    bufferstream pik_info, pik_algorithm_id, pik;
    X509::_load_pik_info(pik_stream, cipher_key, pik_info);
    X509::_extract_pik_info(pik_info, pik_algorithm_id, pik);

    DER::decode_integer(pik);                       // version
    typename RSA::mod_t n = DER::decode_integer(pik);
    typename RSA::mod_t e = DER::decode_integer(pik);
    typename RSA::mod_t d = DER::decode_integer(pik);

    const size_t k = (size_t)n.length(false);
    RSA engine(k * 8, SHA::SHA256);

    std::vector<uint8_t> encrypted(k);
    if (cipher.read((char*)encrypted.data(), 0, (std::streamsize)k).gcount() != (std::streamsize)k)
      throw CertificateException("cipher text is not the width of the modulus");

    std::vector<uint8_t> message(k);
    const int64_t length = engine.RSAES_PKCS1_V1_5_Decrypt(n, d, encrypted.data(), message.data());
    if (length < 0) throw CertificateException("decryption failed");

    plain.write((char*)message.data(), (std::streamsize)length);
  }

  void X509::validate_by_puk(binarystream& puk_stream, binarystream& crt_stream)
  {
    const std::string key_bytes = stream_bytes(puk_stream);
    if (key_bytes.empty()) throw CertificateException("no public key");

    std::unique_ptr<BIO, BioFree> bio(BIO_new_mem_buf(key_bytes.data(), (int)key_bytes.size()));
    if (!bio) throw CertificateException("out of memory reading a public key");

    EVP_PKEY* raw = ::d2i_PUBKEY_bio(bio.get(), nullptr);
    if (raw == nullptr) {
      BIO_reset(bio.get());
      raw = ::PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr);
    }
    if (raw == nullptr) throw CertificateException("not a public key");
    std::unique_ptr<EVP_PKEY, KeyFree> key(raw);

    auto certificate = read_certificate(crt_stream);

    // One call, and it covers what three hand written steps used to: pulling
    // the tbsCertificate back out as the exact octets that were signed, reading
    // the algorithm, and checking the signature over them.
    if (::X509_verify(certificate.get(), key.get()) != 1)
      throw CertificateException("the certificate was not signed by this key");
  }

}
