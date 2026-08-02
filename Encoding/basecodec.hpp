
#ifndef __BASECODEC_HPP__
#define __BASECODEC_HPP__

// Internal to the Encoding project: the RFC 4648 base-N core shared by
// base16/base32/base32hex/base64/base64url. Not installed as a public header.

#include "encodingexception.hpp"
#include <string>
#include <cstring>
#include <cstdint>

namespace Zigurat
{
  namespace BaseCodec
  {

    static const char PADDING = '=';

    // Number of encoded characters produced by `length` octets, padding included.
    inline size_t encoded_size(size_t length, int bits_per_char)
    {
      const size_t group_in  = (bits_per_char == 6) ? 3 : ((bits_per_char == 5) ? 5 : 1);
      const size_t group_out = (bits_per_char == 6) ? 4 : ((bits_per_char == 5) ? 8 : 2);
      return ((length + group_in - 1) / group_in) * group_out;
    }

    // Upper bound on the octets `length` encoded characters can decode to.
    inline size_t decoded_size(size_t length, int bits_per_char)
    {
      return (length * (size_t)bits_per_char) / 8;
    }

    // A 256 entry reverse map, -1 for anything outside the alphabet.
    struct Reverse
    {
      int8_t table[256];

      Reverse(const char* alphabet, size_t size)
      {
	std::memset(this->table, -1, sizeof(this->table));
	for (size_t i = 0; i < size; i++)
	  this->table[(uint8_t)alphabet[i]] = (int8_t)i;
      }
    };

    inline void encode(const char* alphabet, int bits_per_char,
		       const uint8_t* input, size_t length, uint8_t* output)
    {
      const size_t group_in  = (bits_per_char == 6) ? 3 : ((bits_per_char == 5) ? 5 : 1);
      const size_t group_out = (bits_per_char == 6) ? 4 : ((bits_per_char == 5) ? 8 : 2);
      const uint32_t mask = (1u << bits_per_char) - 1;

      size_t out = 0;

      for (size_t i = 0; i < length; i += group_in) {
	const size_t rest = length - i;
	const size_t taken = (rest < group_in) ? rest : group_in;

	// Pack the group left aligned, then peel it off from the top.
	uint64_t accumulator = 0;
	for (size_t j = 0; j < group_in; j++)
	  accumulator = (accumulator << 8) | ((j < taken) ? input[i + j] : 0);

	// Characters that carry at least one real input bit.
	const size_t significant = ((taken * 8) + bits_per_char - 1) / bits_per_char;

	for (size_t j = 0; j < group_out; j++) {
	  if (j < significant) {
	    const int shift = (int)((group_in * 8) - ((j + 1) * bits_per_char));
	    output[out++] = (uint8_t)alphabet[(accumulator >> shift) & mask];
	  } else {
	    output[out++] = (uint8_t)PADDING;
	  }
	}
      }
    }

    // Returns the number of octets written, which padding makes smaller than
    // decoded_size() for a partial final group.
    inline size_t decode(const Reverse& reverse, int bits_per_char, const char* name,
			 const uint8_t* input, size_t length, uint8_t* output)
    {
      size_t out = 0;
      uint32_t accumulator = 0;
      int bits = 0;
      bool padded = false;

      for (size_t i = 0; i < length; i++) {
	const uint8_t ch = input[i];

	if (ch == (uint8_t)PADDING) { padded = true; continue; }

	// Padding only ever appears at the end of the last group.
	if (padded) throw EncodingException(std::string(name) + " wrong encoded input");

	const int8_t value = reverse.table[ch];
	if (value < 0) throw EncodingException(std::string(name) + " wrong encoded input");

	accumulator = (accumulator << bits_per_char) | (uint32_t)value;
	bits += bits_per_char;

	if (bits >= 8) {
	  bits -= 8;
	  output[out++] = (uint8_t)((accumulator >> bits) & 0xFFu);
	}
      }

      return out;
    }

    inline std::string encode_string(const char* alphabet, int bits_per_char, const std::string& input)
    {
      if (input.empty()) return std::string();
      std::string output(BaseCodec::encoded_size(input.size(), bits_per_char), '\0');
      BaseCodec::encode(alphabet, bits_per_char, (const uint8_t*)input.data(), input.size(), (uint8_t*)&output[0]);
      return output;
    }

    inline std::string decode_string(const Reverse& reverse, int bits_per_char, const char* name,
				     const std::string& input)
    {
      if (input.empty()) return std::string();
      std::string output(BaseCodec::decoded_size(input.size(), bits_per_char) + 1, '\0');
      const size_t written = BaseCodec::decode(reverse, bits_per_char, name,
					       (const uint8_t*)input.data(), input.size(), (uint8_t*)&output[0]);
      output.resize(written);
      return output;
    }

  }
}

#endif // __BASECODEC_HPP__
