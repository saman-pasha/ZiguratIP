#include "base32hex.hpp"
#include "basecodec.hpp"
#include "encodingexception.hpp"
#include "bufferstream.hpp"
#include <cmath>


namespace Zigurat
{

  const char Base32Hex::index[32] = {'0', '1', '2', '3', '4', '5', '6', '7', 
				     '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 
				     'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
				     'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V'};

  namespace
  {
    const BaseCodec::Reverse& Base32Hex_reverse()
    {
      static const BaseCodec::Reverse reverse(Base32Hex::index, sizeof(Base32Hex::index));
      return reverse;
    }
  }

  void Base32Hex::encode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::encode(Base32Hex::index, 5, input, length, output);
  }

  void Base32Hex::encode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string encoded = Base32Hex::encode(buffer);
    output.write(encoded.data(), (std::streamsize)encoded.size());
  }

  std::string Base32Hex::encode(const std::string& input)
  {
    return BaseCodec::encode_string(Base32Hex::index, 5, input);
  }

  size_t Base32Hex::encode_size(size_t length)
  {
    return BaseCodec::encoded_size(length, 5);
  }

  void Base32Hex::decode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::decode(Base32Hex_reverse(), 5, "base32hex", input, length, output);
  }

  void Base32Hex::decode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string decoded = Base32Hex::decode(buffer);
    output.write(decoded.data(), (std::streamsize)decoded.size());
  }

  std::string Base32Hex::decode(const std::string& input)
  {
    return BaseCodec::decode_string(Base32Hex_reverse(), 5, "base32hex", input);
  }

  size_t Base32Hex::decode_size(size_t length)
  {
    return BaseCodec::decoded_size(length, 5);
  }

}
