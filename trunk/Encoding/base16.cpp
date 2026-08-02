#include "base16.h"
#include "basecodec.h"
#include "encodingexception.h"
#include "bufferstream.h"


namespace Zigurat
{

  const char Base16::index[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
				  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  namespace
  {
    const BaseCodec::Reverse& Base16_reverse()
    {
      static const BaseCodec::Reverse reverse(Base16::index, sizeof(Base16::index));
      return reverse;
    }
  }

  void Base16::encode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::encode(Base16::index, 4, input, length, output);
  }

  void Base16::encode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string encoded = Base16::encode(buffer);
    output.write(encoded.data(), (std::streamsize)encoded.size());
  }

  std::string Base16::encode(const std::string& input)
  {
    return BaseCodec::encode_string(Base16::index, 4, input);
  }

  size_t Base16::encode_size(size_t length)
  {
    return BaseCodec::encoded_size(length, 4);
  }

  void Base16::decode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::decode(Base16_reverse(), 4, "base16", input, length, output);
  }

  void Base16::decode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string decoded = Base16::decode(buffer);
    output.write(decoded.data(), (std::streamsize)decoded.size());
  }

  std::string Base16::decode(const std::string& input)
  {
    return BaseCodec::decode_string(Base16_reverse(), 4, "base16", input);
  }

  size_t Base16::decode_size(size_t length)
  {
    return BaseCodec::decoded_size(length, 4);
  }

}
