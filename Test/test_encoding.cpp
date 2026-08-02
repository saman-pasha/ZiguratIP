#include "ztest.hpp"
#include "base16.hpp"
#include "base32.hpp"
#include "base32hex.hpp"
#include "base64.hpp"
#include "base64url.hpp"
#include "cte.hpp"
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
