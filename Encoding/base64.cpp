#include "base64.h"
#include "basecodec.h"
#include "encodingexception.h"
#include "bufferstream.h"
#include <cmath>


namespace Zigurat
{

  const char Base64::index[64] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 
				  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 
				  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
				  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
				  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
				  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
				  'w', 'x', 'y', 'z', '0', '1', '2', '3', 
				  '4', '5', '6', '7', '8', '9', '+', '/'};

  namespace
  {
    const BaseCodec::Reverse& Base64_reverse()
    {
      static const BaseCodec::Reverse reverse(Base64::index, sizeof(Base64::index));
      return reverse;
    }
  }

  void Base64::encode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::encode(Base64::index, 6, input, length, output);
  }

  void Base64::encode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string encoded = Base64::encode(buffer);
    output.write(encoded.data(), (std::streamsize)encoded.size());
  }

  std::string Base64::encode(const std::string& input)
  {
    return BaseCodec::encode_string(Base64::index, 6, input);
  }

  size_t Base64::encode_size(size_t length)
  {
    return BaseCodec::encoded_size(length, 6);
  }

  void Base64::decode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::decode(Base64_reverse(), 6, "base64", input, length, output);
  }

  void Base64::decode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string decoded = Base64::decode(buffer);
    output.write(decoded.data(), (std::streamsize)decoded.size());
  }

  std::string Base64::decode(const std::string& input)
  {
    return BaseCodec::decode_string(Base64_reverse(), 6, "base64", input);
  }

  size_t Base64::decode_size(size_t length)
  {
    return BaseCodec::decoded_size(length, 6);
  }

}
