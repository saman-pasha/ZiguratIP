#include "base64url.hpp"
#include "basecodec.hpp"
#include "encodingexception.hpp"
#include "bufferstream.hpp"
#include <cmath>


namespace Zigurat
{

  const char Base64URL::index[64] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
				     'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 
				     'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
				     'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
				     'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
				     'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
				     'w', 'x', 'y', 'z', '0', '1', '2', '3', 
				     '4', '5', '6', '7', '8', '9', '-', '_'};

  namespace
  {
    const BaseCodec::Reverse& Base64URL_reverse()
    {
      static const BaseCodec::Reverse reverse(Base64URL::index, sizeof(Base64URL::index));
      return reverse;
    }
  }

  void Base64URL::encode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::encode(Base64URL::index, 6, input, length, output);
  }

  void Base64URL::encode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string encoded = Base64URL::encode(buffer);
    output.write(encoded.data(), (std::streamsize)encoded.size());
  }

  std::string Base64URL::encode(const std::string& input)
  {
    return BaseCodec::encode_string(Base64URL::index, 6, input);
  }

  size_t Base64URL::encode_size(size_t length)
  {
    return BaseCodec::encoded_size(length, 6);
  }

  void Base64URL::decode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::decode(Base64URL_reverse(), 6, "base64url", input, length, output);
  }

  void Base64URL::decode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string decoded = Base64URL::decode(buffer);
    output.write(decoded.data(), (std::streamsize)decoded.size());
  }

  std::string Base64URL::decode(const std::string& input)
  {
    return BaseCodec::decode_string(Base64URL_reverse(), 6, "base64url", input);
  }

  size_t Base64URL::decode_size(size_t length)
  {
    return BaseCodec::decoded_size(length, 6);
  }

}
