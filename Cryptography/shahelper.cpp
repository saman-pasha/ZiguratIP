#include "shahelper.hpp"
#include "sha.hpp"
#include "cryptographyexception.hpp"
#include "utility.hpp"


namespace Zigurat
{

  void SHA::checksum(SHA::Version version, const uint8_t* octets, size_t length, uint8_t* digest)
  {
    if (version == SHA::Version::SHA1) {
      SHA1Context context;
      if (SHA1Reset(&context) > 0)               throw CryptographyException("sha error");
      if (SHA1Input(&context, octets, length))   throw CryptographyException("sha error");
      if (SHA1Result(&context, digest) > 0)      throw CryptographyException("sha error");
    } else if (version == SHA::Version::SHA224) {
      SHA224Context context;
      if (SHA224Reset(&context) > 0)             throw CryptographyException("sha error");
      if (SHA224Input(&context, octets, length)) throw CryptographyException("sha error");
      if (SHA224Result(&context, digest) > 0)    throw CryptographyException("sha error");
    } else if (version == SHA::Version::SHA256) {
      SHA256Context context;
      if (SHA256Reset(&context) > 0)             throw CryptographyException("sha error");
      if (SHA256Input(&context, octets, length)) throw CryptographyException("sha error");
      if (SHA256Result(&context, digest) > 0)    throw CryptographyException("sha error");
    } else if (version == SHA::Version::SHA384) {
      SHA384Context context;
      if (SHA384Reset(&context) > 0)             throw CryptographyException("sha error");
      if (SHA384Input(&context, octets, length)) throw CryptographyException("sha error");
      if (SHA384Result(&context, digest) > 0)    throw CryptographyException("sha error");
    } else if (version == SHA::Version::SHA512) {
      SHA512Context context;
      if (SHA512Reset(&context) > 0)             throw CryptographyException("sha error");
      if (SHA512Input(&context, octets, length)) throw CryptographyException("sha error");
      if (SHA512Result(&context, digest) > 0)    throw CryptographyException("sha error");
    } else {
      throw CryptographyException("sha unknown version");
    }
  }

  void SHA::checksum(Version version, std::istream& octets, size_t length, std::ostream& digest)
  {
    uint8_t octets_buffer[length];
    octets.read((char*)octets_buffer, length);
    
    size_t  digest_length = SHA::size(version);
    uint8_t digest_buffer[digest_length];
  
    SHA::checksum(version, octets_buffer, length, digest_buffer);
  
    digest.write((char*)digest_buffer, digest_length);
  }

  std::string SHA::checksum(Version version, const std::string& data)
  {
    size_t  digest_length = SHA::size(version);
    uint8_t digest_buffer[digest_length];
    
    SHA::checksum(version, (const uint8_t*)data.c_str(), data.length(), digest_buffer);

    return Utility::octet_as_hex(digest_buffer, digest_length);
  }

  void SHA::hmac(Version version, const uint8_t* key, size_t key_len, const uint8_t* text, size_t text_len, uint8_t* digest)
  {
    switch (version) {
    case SHA::Version::SHA1:   ::hmac(SHAversion::SHA1  , text, text_len, key, key_len, digest); break;
    case SHA::Version::SHA224: ::hmac(SHAversion::SHA224, text, text_len, key, key_len, digest); break;
    case SHA::Version::SHA256: ::hmac(SHAversion::SHA256, text, text_len, key, key_len, digest); break;
    case SHA::Version::SHA384: ::hmac(SHAversion::SHA384, text, text_len, key, key_len, digest); break;
    case SHA::Version::SHA512: ::hmac(SHAversion::SHA512, text, text_len, key, key_len, digest); break;
    default: throw CryptographyException("sha unknown version");
    }
  }

  void SHA::hmac(Version version, std::istream& key, size_t key_length, std::istream& text, size_t text_length, std::ostream& digest)
  {
    uint8_t key_buffer[key_length];
    key.read((char*)key_buffer, key_length);

    uint8_t text_buffer[text_length];
    text.read((char*)text_buffer, text_length);

    size_t  digest_length = SHA::size(version);
    uint8_t digest_buffer[digest_length];

    SHA::hmac(version, key_buffer, key_length, text_buffer, text_length, digest_buffer);

    digest.write((char*)digest_buffer, digest_length);
  }

  size_t SHA::size(SHA::Version version)
  {
    switch (version) {
    case SHA::Version::SHA1:   return SHA1HashSize;
    case SHA::Version::SHA224: return SHA224HashSize;
    case SHA::Version::SHA256: return SHA256HashSize;
    case SHA::Version::SHA384: return SHA384HashSize;
    case SHA::Version::SHA512: return SHA512HashSize;
    default: throw CryptographyException("sha unknown version");
    }
  }

  std::string SHA::name(SHA::Version version)
  {
    switch (version) {
    case SHA::Version::SHA1:   return "SHA1";
    case SHA::Version::SHA224: return "SHA224";
    case SHA::Version::SHA256: return "SHA256";
    case SHA::Version::SHA384: return "SHA384";
    case SHA::Version::SHA512: return "SHA512";
    default: throw CryptographyException("sha unknown version");
    }
  }

}
