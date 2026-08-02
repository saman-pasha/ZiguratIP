#include "cte.hpp"
#include "base16.hpp"
#include "base32.hpp"
#include "base32hex.hpp"
#include "base64.hpp"
#include "base64url.hpp"
#include "encodingexception.hpp"


namespace Zigurat
{

  void CTE::encode(CTE::Scheme scheme, binarystream& input, size_t length, binarystream& output)
  {
    switch (scheme) {
    case Scheme::HEX:
    case Scheme::BASE16:
      Base16::encode(input, length, output);
      break;
    case Scheme::BASE32:
      Base32::encode(input, length, output);
      break;
    case Scheme::BASE32HEX:
      Base32Hex::encode(input, length, output);
      break;
    case Scheme::BASE64:
      Base64::encode(input, length, output);
      break;
    case Scheme::BASE64URL:
      Base64URL::encode(input, length, output);
      break;
    case Scheme::MIME:
      {
        bufferstream tmp;
        char buffer[76];
	size_t pre_len = 0, count = 0;
	Base64::encode(input, length, tmp);
	while (!tmp.eof()) {
	  tmp.read(buffer, 76);
	  count = tmp.gcount();
	  if (pre_len == 76 && count > 0) output.put('\n');
	  pre_len = count;
	  output.write(buffer, count);
	}
      }
      break;
    case Scheme::PEM:
      {
        bufferstream tmp;
	char buffer[64];
	size_t pre_len = 0, count = 0;
	Base64::encode(input, length, tmp);
	while (!tmp.eof()) {
	  tmp.read(buffer, 64);
	  count = tmp.gcount();
	  if (pre_len == 64 && count > 0) output.put('\n');
	  pre_len = count;
	  output.write(buffer, count);
	}
      }
      break;
    }
  }

  void CTE::decode(CTE::Scheme scheme, binarystream& input, size_t length, binarystream& output)
  {
    switch (scheme) {
    case Scheme::HEX:
    case Scheme::BASE16:
      Base16::decode(input, length, output);
      break;
    case Scheme::BASE32:
      Base32::decode(input, length, output);
      break;
    case Scheme::BASE32HEX:
      Base32Hex::decode(input, length, output);
      break;
    case Scheme::BASE64:
      Base64::decode(input, length, output);
      break;
    case Scheme::BASE64URL:
      Base64URL::decode(input, length, output);
      break;
    case Scheme::MIME:
      {
        bufferstream tmp;
	std::streampos count = 76;
	// tellp, not tellg: this measures how much the line just appended to
	// tmp, and tmp is only ever written to here. Reading the get position
	// left count at zero, so the loop stopped after the first line.
	while (!input.eof() && count == 76) {
	  count = tmp.tellp();
	  input.getline(tmp, 77);
	  count = tmp.tellp() - count;
	  if (count > 76) throw EncodingException("MIME wrong encoded input");
	}
	Base64::decode(tmp, tmp.length(), output);
      }
      break;
    case Scheme::PEM:
      {
        bufferstream tmp;
	std::streampos count = 64;
	// tellp, not tellg: this measures how much the line just appended to
	// tmp, and tmp is only ever written to here. Reading the get position
	// left count at zero, so the loop stopped after the first line.
	while (!input.eof() && count == 64) {
	  count = tmp.tellp();
	  input.getline(tmp, 65);
	  count = tmp.tellp() - count;
	  if (count > 64) throw EncodingException("PEM wrong encoded input");
	}
	Base64::decode(tmp, tmp.length(), output);
      }
      break;
    }
  }

}
