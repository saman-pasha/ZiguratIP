#include "ztest.hpp"
#include "base16.hpp"
#include "base32.hpp"
#include "base32hex.hpp"
#include "base64.hpp"
#include "base64url.hpp"
#include "cte.hpp"
#include "der.hpp"
#include "bigint.hpp"
#include "bufferstream.hpp"
#include "utility.hpp"
#include <cstring>

using namespace Zigurat;


// The RFC 4648 vectors, which every one of these alphabets is specified by.
ZTEST(Encoding, base16_rfc4648_vectors)
{
  ZCHECK_STR(Utility::to_upper(Base16::encode("")), "");
  ZCHECK_STR(Utility::to_upper(Base16::encode("f")), "66");
  ZCHECK_STR(Utility::to_upper(Base16::encode("fo")), "666F");
  ZCHECK_STR(Utility::to_upper(Base16::encode("foo")), "666F6F");
  ZCHECK_STR(Utility::to_upper(Base16::encode("foobar")), "666F6F626172");
}

ZTEST(Encoding, base16_roundtrip)
{
  const std::string data = "ZiguratIP database";
  ZCHECK_STR(Base16::decode(Base16::encode(data)), data);
}

ZTEST(Encoding, base64_rfc4648_vectors)
{
  ZCHECK_STR(Base64::encode("f"), "Zg==");
  ZCHECK_STR(Base64::encode("fo"), "Zm8=");
  ZCHECK_STR(Base64::encode("foo"), "Zm9v");
  ZCHECK_STR(Base64::encode("foob"), "Zm9vYg==");
  ZCHECK_STR(Base64::encode("fooba"), "Zm9vYmE=");
  ZCHECK_STR(Base64::encode("foobar"), "Zm9vYmFy");
}

ZTEST(Encoding, base64_roundtrip)
{
  const std::string data = "ZiguratIP database";
  ZCHECK_STR(Base64::decode(Base64::encode(data)), data);
}

ZTEST(Encoding, base64_size_helpers)
{
  // 3 input octets become 4 output characters, rounded up.
  ZCHECK(Base64::encode_size(3) >= 4);
  ZCHECK(Base64::encode_size(1) >= 4);
  ZCHECK(Base64::decode_size(4) >= 3);
}

ZTEST(Encoding, base32_rfc4648_vectors)
{
  ZCHECK_STR(Base32::encode("f"), "MY======");
  ZCHECK_STR(Base32::encode("fo"), "MZXQ====");
  ZCHECK_STR(Base32::encode("foo"), "MZXW6===");
  ZCHECK_STR(Base32::encode("foobar"), "MZXW6YTBOI======");
}

ZTEST(Encoding, base32_roundtrip)
{
  const std::string data = "ZiguratIP";
  ZCHECK_STR(Base32::decode(Base32::encode(data)), data);
}

ZTEST(Encoding, base32hex_roundtrip)
{
  const std::string data = "ZiguratIP";
  ZCHECK_STR(Base32Hex::decode(Base32Hex::encode(data)), data);
}

ZTEST(Encoding, base64url_uses_the_url_safe_alphabet)
{
  // 0xFB 0xFF encodes to characters 62 and 63, which is where the two
  // alphabets differ: + / in base64, - _ in base64url.
  const std::string data("\xfb\xff\xfe", 3);
  const std::string standard = Base64::encode(data);
  const std::string url = Base64URL::encode(data);

  ZCHECK(standard != url);
  ZCHECK(url.find('+') == std::string::npos);
  ZCHECK(url.find('/') == std::string::npos);
  ZCHECK_STR(Base64URL::decode(url), data);
}

ZTEST(Encoding, cte_stream_roundtrip)
{
  const std::string data = "content transfer encoding";

  bufferstream input(data);
  bufferstream encoded;
  CTE::encode(CTE::BASE64, input, data.size(), encoded);
  ZCHECK(encoded.string().size() > 0);

  bufferstream to_decode(encoded.string());
  bufferstream decoded;
  CTE::decode(CTE::BASE64, to_decode, encoded.string().size(), decoded);

  ZCHECK_STR(decoded.string(), data);
}

ZTEST(Encoding, binary_data_survives_base64)
{
  std::string binary;
  for (int i = 0; i < 256; i++) binary.push_back((char)i);

  const std::string encoded = Base64::encode(binary);
  ZCHECK_STR(Base64::decode(encoded), binary);
}


// ---------------------------------------------------------------------------
// DER, as other implementations read it
// ---------------------------------------------------------------------------

namespace
{
  // The encoded INTEGER as hex, so a wrong length or a stray pad byte shows up
  // in the failure message rather than as a silent mismatch.
  std::string der_integer(const BigInt& value)
  {
    bufferstream out;
    DER::encode_integer(out, value);
    std::string hex;
    for (std::streamsize i = 0; i < out.length(); i++) {
      static const char* digits = "0123456789ABCDEF";
      const uint8_t byte = (uint8_t)out.at(i);
      hex += digits[byte >> 4];
      hex += digits[byte & 0x0F];
    }
    return hex;
  }
}

// X.690 8.3.2: the shortest form, with a leading 00 only where the next octet
// would otherwise read as negative. BigInt works in 32 bit words and used to
// hand the padding straight through -- 1001 went out as 00 00 03 E9, and
// OpenSSL called it a BAD INTEGER and refused the whole certificate.
ZTEST(Encoding, der_integers_are_minimally_encoded)
{
  ZCHECK_STR(der_integer(BigInt(1)), "020101");
  ZCHECK_STR(der_integer(BigInt(127)), "02017F");
  ZCHECK_STR(der_integer(BigInt(1001)), "020203E9");
  ZCHECK_STR(der_integer(BigInt(65537)), "0203010001");        // the usual RSA exponent

  // 128 needs the sign octet: 80 alone would read as negative.
  ZCHECK_STR(der_integer(BigInt(128)), "02020080");
  ZCHECK_STR(der_integer(BigInt(255)), "020200FF");
  ZCHECK_STR(der_integer(BigInt(256)), "02020100");
}

// The other half of the same fix: a minimal integer is rarely a whole number of
// words, and the octet constructor used to read four bytes at a time from the
// front of the array. Anything not filling the last word was dropped.
ZTEST(Encoding, bigints_survive_octet_lengths_that_are_not_whole_words)
{
  const uint64_t values[] = {1, 127, 128, 255, 256, 1001, 65537, 16777216, 4294967296ULL};

  for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
    bufferstream encoded;
    DER::encode_integer(encoded, BigInt(values[i]));

    DER::Integer decoded = DER::decode_integer(encoded);
    ZCHECK_EQ((uint64_t)decoded, values[i]);

    // And through BigInt, which is the path an RSA modulus takes.
    const BigInt round_trip = decoded.operator BigInt();
    bufferstream again;
    DER::encode_integer(again, round_trip);
    ZCHECK_EQ(again.length(), encoded.length());
  }
}
