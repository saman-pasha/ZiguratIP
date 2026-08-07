#include "shahelper.hpp"
#include "cryptographyexception.hpp"
#include "utility.hpp"
#include <vector>
#include <openssl/evp.h>
#include <openssl/hmac.h>


namespace Zigurat
{

  namespace
  {
    // The digests were five files of hand-written SHA -- the reference code
    // from RFC 6234, carried in the tree and maintained here. They computed
    // the right answers, which is the least that can be asked of a digest and
    // not much of an argument for keeping them: this is exactly the code that
    // ought to be somebody else's, reviewed by more people than have ever read
    // this repository.
    //
    // The output is unchanged, and has to be. SHA-1 digests name the B-tree
    // index files under home/data and the compiled objects under home/ld, so a
    // different byte here does not produce a wrong answer -- it produces a
    // store whose indexes cannot be found.
    const EVP_MD* digest_of(SHA::Version version)
    {
      switch (version) {
      case SHA::Version::SHA1:   return EVP_sha1();
      case SHA::Version::SHA224: return EVP_sha224();
      case SHA::Version::SHA256: return EVP_sha256();
      case SHA::Version::SHA384: return EVP_sha384();
      case SHA::Version::SHA512: return EVP_sha512();
      default: throw CryptographyException("sha unknown version");
      }
    }

    // Reading a caller's length into a buffer, without putting that length on
    // the stack. The stream overloads used to say uint8_t buffer[length] with
    // the length passed in by whoever called them -- the same array-of-unknown-
    // size that killed the server when it was asked for a large file.
    const size_t CHUNK = 64 * 1024;
  }

  void SHA::checksum(SHA::Version version, const uint8_t* octets, size_t length, uint8_t* digest)
  {
    if (octets == nullptr && length > 0) throw CryptographyException("sha given no data");

    unsigned int written = 0;
    if (EVP_Digest(octets, length, digest, &written, digest_of(version), nullptr) != 1)
      throw CryptographyException("sha error");
    if (written != SHA::size(version)) throw CryptographyException("sha error");
  }

  void SHA::checksum(Version version, std::istream& octets, size_t length, std::ostream& digest)
  {
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (context == nullptr) throw CryptographyException("sha error");

    try {
      if (EVP_DigestInit_ex(context, digest_of(version), nullptr) != 1)
	throw CryptographyException("sha error");

      // Fed through in fixed pieces, so a length of any size costs the same
      // sixty-four kilobytes rather than that much stack.
      std::vector<uint8_t> buffer(CHUNK);
      size_t remaining = length;
      while (remaining > 0) {
	const size_t want = (remaining < CHUNK) ? remaining : CHUNK;
	octets.read((char*)buffer.data(), want);
	const std::streamsize got = octets.gcount();
	if (got <= 0) throw CryptographyException("sha short read");
	if (EVP_DigestUpdate(context, buffer.data(), (size_t)got) != 1)
	  throw CryptographyException("sha error");
	remaining -= (size_t)got;
      }

      uint8_t out[EVP_MAX_MD_SIZE];
      unsigned int written = 0;
      if (EVP_DigestFinal_ex(context, out, &written) != 1)
	throw CryptographyException("sha error");

      EVP_MD_CTX_free(context);
      digest.write((char*)out, written);
    } catch (...) {
      EVP_MD_CTX_free(context);
      throw;
    }
  }

  std::string SHA::checksum(Version version, const std::string& data)
  {
    uint8_t digest_buffer[EVP_MAX_MD_SIZE];
    SHA::checksum(version, (const uint8_t*)data.c_str(), data.length(), digest_buffer);
    return Utility::octet_as_hex(digest_buffer, SHA::size(version));
  }

  void SHA::hmac(Version version, const uint8_t* key, size_t key_len, const uint8_t* text, size_t text_len, uint8_t* digest)
  {
    unsigned int written = 0;
    if (::HMAC(digest_of(version), key, (int)key_len, text, text_len, digest, &written) == nullptr)
      throw CryptographyException("hmac error");
    if (written != SHA::size(version)) throw CryptographyException("hmac error");
  }

  void SHA::hmac(Version version, std::istream& key, size_t key_length, std::istream& text, size_t text_length, std::ostream& digest)
  {
    // Both of these were arrays sized by the caller's length as well.
    std::vector<uint8_t> key_buffer(key_length);
    if (key_length > 0) {
      key.read((char*)key_buffer.data(), key_length);
      if ((size_t)key.gcount() < key_length) throw CryptographyException("hmac short read");
    }

    std::vector<uint8_t> text_buffer(text_length);
    if (text_length > 0) {
      text.read((char*)text_buffer.data(), text_length);
      if ((size_t)text.gcount() < text_length) throw CryptographyException("hmac short read");
    }

    uint8_t digest_buffer[EVP_MAX_MD_SIZE];
    SHA::hmac(version, key_buffer.data(), key_length, text_buffer.data(), text_length, digest_buffer);

    digest.write((char*)digest_buffer, SHA::size(version));
  }

  // Written out rather than asked of EVP_MD_get_size: these are fixed by the
  // standard and will not change, and they are wanted before a context exists.
  size_t SHA::size(SHA::Version version)
  {
    switch (version) {
    case SHA::Version::SHA1:   return 20;
    case SHA::Version::SHA224: return 28;
    case SHA::Version::SHA256: return 32;
    case SHA::Version::SHA384: return 48;
    case SHA::Version::SHA512: return 64;
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
