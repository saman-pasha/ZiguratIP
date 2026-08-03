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


namespace Zigurat
{

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

    bufferstream certificate;
    X509::_load_certificate(crt_stream, certificate);

    bufferstream tbs;
    DER::decode_sequence(certificate, tbs);

    bufferstream version;
    if (!DER::decode_context(tbs, version, 0))
      return permissions;                       // v1: there is nowhere for them to be

    // Past serial, signature, issuer, validity, subject and the key, to [3].
    for (int i = 0; i < 6; i++) {
      if (tbs.tellg() >= tbs.length()) return permissions;
      bufferstream skipped;
      DER::decode_tlv(tbs, skipped);
    }

    bufferstream wrapper, extensions;
    if (!DER::decode_context(tbs, wrapper, 3)) return permissions;
    DER::decode_sequence(wrapper, extensions);

    const DER::oid_t wanted = permissions_oid();

    while (extensions.tellg() < extensions.length()) {

      bufferstream extension;
      DER::decode_sequence(extensions, extension);

      DER::oid_t oid = DER::decode_oid(extension);
      if (oid != wanted) continue;              // some other extension: not ours to read

      // critical is optional and defaults to false, so it may or may not be
      // sitting between the OID and the payload.
      bufferstream payload;
      const std::streampos mark = extension.tellg();
      try {
	DER::decode_octet_string(extension, payload);
      } catch (const std::exception&) {
	extension.clear();
	extension.seekg(mark, std::ios::beg);
	DER::decode_boolean(extension);
	payload.string("");
	DER::decode_octet_string(extension, payload);
      }

      bufferstream granted;
      DER::decode_sequence(payload, granted);

      // decode_utf8_string writes into an array, so the length is read from the
      // header first to know how much room to give it.
      while (granted.tellg() < granted.length()) {
	const std::streampos mark = granted.tellg();
	DER::decode_tag(granted);
	const size_t length = DER::decode_length(granted);
	granted.clear();
	granted.seekg(mark, std::ios::beg);

	std::vector<uint8_t> text(length + 1, 0);
	DER::decode_utf8_string(granted, text.data());
	permissions.push_back(std::string((const char*)text.data(), length));
      }
    }

    return permissions;
  }

  void X509::certificate_public_key(binarystream& crt_stream, binarystream& puk_info)
  {
    bufferstream certificate;
    X509::_load_certificate(crt_stream, certificate);

    bufferstream spki;
    _tbs_element(certificate, 5, spki);          // subjectPublicKeyInfo

    DER::decode_sequence(spki, puk_info);
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
    bufferstream certificate;
    X509::_load_certificate(crt_stream, certificate);

    bufferstream subject_tlv;
    _tbs_element(certificate, 4, subject_tlv);   // subject

    bufferstream rdns;
    DER::decode_sequence(subject_tlv, rdns);

    std::string name;
    while (rdns.length() > rdns.tellg()) {

      bufferstream rdn, attribute;
      DER::decode_set(rdns, rdn);
      DER::decode_sequence(rdn, attribute);

      DER::oid_t oid = DER::decode_oid(attribute);

      bufferstream value;
      DER::decode_tlv(attribute, value);
      DER::decode_tag(value);
      const size_t length = DER::decode_length(value);
      std::string text((size_t)length, 0);
      if (length > 0) value.read(&text[0], (std::streamsize)length);

      if (!name.empty()) name += ", ";
      name += _attribute_name(oid) + "=" + text;
    }

    return name;
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
    bufferstream puk_info;
    X509::_load_puk_info(puk_stream, puk_info);

    bufferstream certificate;
    X509::_load_certificate(crt_stream, certificate);

    bufferstream tbs_certificate_data, signature_algorithm_id, signature_value;
    DER::decode_tlv(certificate, tbs_certificate_data);
    DER::decode_sequence(certificate, signature_algorithm_id);
    DER::decode_bit_string(certificate, signature_value);

    X509::_verify(puk_info, signature_algorithm_id, tbs_certificate_data, signature_value);
  }

}
