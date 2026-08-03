#include "ztest.hpp"
#include "shahelper.hpp"
#include "rsa.hpp"
#include "utility.hpp"
#include "bufferstream.hpp"
#include <cstring>

using namespace Zigurat;


// Known answers from FIPS 180-4 / RFC 3174, so a refactor of the digest cores
// cannot quietly change the output.
ZTEST(Cryptography, sha1_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA1, "")),
	     "da39a3ee5e6b4b0d3255bfef95601890afd80709");
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA1, "abc")),
	     "a9993e364706816aba3e25717850c26c9cd0d89d");
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA1, "The quick brown fox jumps over the lazy dog")),
	     "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}

ZTEST(Cryptography, sha224_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA224, "")),
	     "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f");
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA224, "abc")),
	     "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7");
}

ZTEST(Cryptography, sha256_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA256, "")),
	     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA256, "abc")),
	     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

ZTEST(Cryptography, sha384_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA384, "abc")),
	     "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
	     "8086072ba1e7cc2358baeca134c825a7");
}

ZTEST(Cryptography, sha512_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA512, "abc")),
	     "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
	     "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
}

ZTEST(Cryptography, digest_sizes)
{
  ZCHECK_EQ(SHA::size(SHA::SHA1), (size_t)20);
  ZCHECK_EQ(SHA::size(SHA::SHA224), (size_t)28);
  ZCHECK_EQ(SHA::size(SHA::SHA256), (size_t)32);
  ZCHECK_EQ(SHA::size(SHA::SHA384), (size_t)48);
  ZCHECK_EQ(SHA::size(SHA::SHA512), (size_t)64);
  ZCHECK(SHA::name(SHA::SHA256).size() > 0);
}

ZTEST(Cryptography, digest_is_stable_and_distinguishing)
{
  const std::string a = SHA::checksum(SHA::SHA256, "ZiguratIP");
  const std::string b = SHA::checksum(SHA::SHA256, "ZiguratIP");
  const std::string c = SHA::checksum(SHA::SHA256, "ZiguratIQ");

  ZCHECK_STR(a, b);
  ZCHECK(a != c);
}

ZTEST(Cryptography, raw_octet_digest_matches_the_string_form)
{
  const std::string data = "abc";
  uint8_t digest[32];
  std::memset(digest, 0, sizeof(digest));

  SHA::checksum(SHA::SHA256, (const uint8_t*)data.c_str(), data.size(), digest);

  ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex(digest, 32)),
	     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

ZTEST(Cryptography, stream_digest_matches_the_buffer_digest)
{
  bufferstream input("abc");
  bufferstream output;

  SHA::checksum(SHA::SHA256, input, 3, output);

  const std::string raw = output.string();
  ZCHECK_EQ(raw.size(), (size_t)32);
  ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex((const uint8_t*)raw.data(), raw.size())),
	     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// RFC 4231 test case 1 (and the RFC 2202 SHA-1 equivalent). Note the argument
// order is (text, key), following the IETF reference implementation.
ZTEST(Cryptography, hmac_known_answers)
{
  uint8_t key[20];
  std::memset(key, 0x0b, sizeof(key));
  const std::string data = "Hi There";

  uint8_t mac256[32];
  std::memset(mac256, 0, sizeof(mac256));
  SHA::hmac(SHA::SHA256, (const uint8_t*)data.c_str(), data.size(), key, sizeof(key), mac256);
  ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex(mac256, 32)),
	     "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

  uint8_t mac1[20];
  std::memset(mac1, 0, sizeof(mac1));
  SHA::hmac(SHA::SHA1, (const uint8_t*)data.c_str(), data.size(), key, sizeof(key), mac1);
  ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex(mac1, 20)),
	     "b617318655057264e28bc0b6fb378c8ef146be00");
}

ZTEST(Cryptography, unknown_digest_version_is_rejected)
{
  ZCHECK_THROWS(SHA::size((SHA::Version)999));
}


// ---------------------------------------------------------------------------
// PKCS #1 v1.5 signature encoding
// ---------------------------------------------------------------------------

// RFC 8017 9.2 spells the DigestInfo prefixes out byte for byte, and they carry
// the OID of the HASH. These used to hold 1.2.840.113549.1.1.x -- the
// sha256WithRSAEncryption family, which belongs in a certificate's
// signatureAlgorithm field. Both OIDs are nine octets, so the block was the
// right shape and verified against itself; every other implementation read the
// DigestInfo, failed to recognise it, and rejected the signature.
ZTEST(Cryptography, pkcs1_v1_5_digest_info_carries_the_hash_oid)
{
  struct Case
  {
    SHA::Version version;
    size_t       prefix_length;
    const char*  prefix;      // hex, from RFC 8017 9.2
  };

  static const Case cases[] = {
    {SHA::SHA1,   15, "3021300906052B0E03021A05000414"},
    {SHA::SHA224, 19, "302D300D06096086480165030402040500041C"},
    {SHA::SHA256, 19, "3031300D060960864801650304020105000420"},
    {SHA::SHA384, 19, "3041300D060960864801650304020205000430"},
    {SHA::SHA512, 19, "3051300D060960864801650304020305000440"}
  };

  const uint8_t message[] = {'a', 'b', 'c'};

  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
    RSA rsa(2048, cases[c].version);

    uint8_t block[256];
    std::memset(block, 0, sizeof(block));
    ZCHECK_NOTHROW(rsa.EMSA_PKCS1_V1_5_Encode(message, sizeof(message), sizeof(block), block));

    // 00 01 then padding, so the DigestInfo begins after the 00 separator.
    ZCHECK_EQ((int)block[0], 0);
    ZCHECK_EQ((int)block[1], 1);

    size_t at = 2;
    while (at < sizeof(block) && block[at] == 0xFF) at++;
    ZCHECK(at > 10);                       // at least 8 octets of padding
    ZCHECK_EQ((int)block[at], 0);
    at++;

    static const char* digits = "0123456789ABCDEF";
    std::string prefix;
    for (size_t i = 0; i < cases[c].prefix_length && at + i < sizeof(block); i++) {
      prefix += digits[block[at + i] >> 4];
      prefix += digits[block[at + i] & 0x0F];
    }
    ZCHECK_STR(prefix, cases[c].prefix);
  }
}
