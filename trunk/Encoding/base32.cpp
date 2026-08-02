#include "base32.h"
#include "basecodec.h"
#include "encodingexception.h"
#include "bufferstream.h"
#include <cmath>


namespace Zigurat
{

  const char Base32::index[32] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
				  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
				  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
				  'Y', 'Z', '2', '3', '4', '5', '6', '7'};

  namespace
  {
    const BaseCodec::Reverse& Base32_reverse()
    {
      static const BaseCodec::Reverse reverse(Base32::index, sizeof(Base32::index));
      return reverse;
    }
  }

  void Base32::encode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::encode(Base32::index, 5, input, length, output);
  }

  void Base32::encode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string encoded = Base32::encode(buffer);
    output.write(encoded.data(), (std::streamsize)encoded.size());
  }

  std::string Base32::encode(const std::string& input)
  {
    return BaseCodec::encode_string(Base32::index, 5, input);
  }

  size_t Base32::encode_size(size_t length)
  {
    return BaseCodec::encoded_size(length, 5);
  }

  void Base32::decode(const uint8_t* input, size_t length, uint8_t* output)
  {
    BaseCodec::decode(Base32_reverse(), 5, "base32", input, length, output);
  }

  void Base32::decode(binarystream& input, size_t length, binarystream& output)
  {
    std::string buffer(length, '\0');
    if (length > 0) input.read(&buffer[0], (std::streamsize)length);
    const std::string decoded = Base32::decode(buffer);
    output.write(decoded.data(), (std::streamsize)decoded.size());
  }

  std::string Base32::decode(const std::string& input)
  {
    return BaseCodec::decode_string(Base32_reverse(), 5, "base32", input);
  }

  size_t Base32::decode_size(size_t length)
  {
    return BaseCodec::decoded_size(length, 5);
  }

}
