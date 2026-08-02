#include "der.hpp"
#include <cstring>
#include <algorithm>
#include "encodingexception.hpp"
#include "utility.hpp"
#include "bufferstream.hpp"


namespace Zigurat
{

  DER::Integer::Integer(size_t length, bool sign)
    : _array(new uint8_t[length]), _length(length), _sign(sign)
  {
    std::memset(this->_array, 0x00, this->_length);
  }

  DER::Integer::Integer(const Integer& other)
    : Integer(other._length, other._sign)
  {
    std::memcpy(this->_array, other._array, this->_length);
  }

  size_t DER::Integer::length() const
  {
    return this->_length;
  }

  bool DER::Integer::has_sign() const
  {
    return this->_sign;
  }

  DER::Integer::operator int8_t() const
  {
    return (int8_t)*(this->_array + this->_length - 1);
  }

  DER::Integer::operator uint8_t() const
  {
    return *(this->_array + this->_length - 1);
  }

  DER::Integer::operator int16_t() const
  {
    int16_t integer = *this->_array;
    for (size_t i = 1; i < this->_length; i++) {
      integer <<= 8;
      integer += *(this->_array + i);
    }
    return integer;
  }

  DER::Integer::operator uint16_t() const
  {
    uint16_t integer = *this->_array;
    for (size_t i = 1; i < this->_length; i++) {
      integer <<= 8;
      integer += *(this->_array + i);
    }
    return integer;
  }

  DER::Integer::operator int32_t() const
  {
    int32_t integer = *this->_array;
    for (size_t i = 1; i < this->_length; i++) {
      integer <<= 8;
      integer += *(this->_array + i);
    }
    return integer;
  }

  DER::Integer::operator uint32_t() const
  {
    uint32_t integer = *this->_array;
    for (size_t i = 1; i < this->_length; i++) {
      integer <<= 8;
      integer += *(this->_array + i);
    }
    return integer;
  }

  DER::Integer::operator int64_t() const
  {
    int64_t integer = *this->_array;
    for (size_t i = 1; i < this->_length; i++) {
      integer <<= 8;
      integer += *(this->_array + i);
    }
    return integer;
  }

  DER::Integer::operator uint64_t() const
  {
    uint64_t integer = *this->_array;
    for (size_t i = 1; i < this->_length; i++) {
      integer <<= 8;
      integer += *(this->_array + i);
    }
    return integer;
  }

  DER::Integer::operator BigInt() const
  {
    return BigInt(this->_array, this->_length, true);
  }

  binarystream& operator<<(binarystream& ostream, const DER::Integer& integer)
  {
    if (integer._sign) {
      switch (integer._length) {
      case 1:
	ostream << (int8_t)integer;
	break;
      case 2:
	ostream << (int16_t)integer;
	break;
      case 3:
	ostream << (int32_t)integer;
	break;
      case 4:
	ostream << (int32_t)integer;
	break;
      case 5:
	ostream << (int64_t)integer;
	break;
      case 6:
	ostream << (int64_t)integer;
	break;
      case 7:
	ostream << (int64_t)integer;
	break;
      case 8:
	ostream << (int64_t)integer;
	break;
      }
    } else {
      switch (integer._length) {
      case 1:
	ostream << (uint8_t)integer;
	break;
      case 2:
	ostream << (uint16_t)integer;
	break;
      case 3:
	ostream << (uint32_t)integer;
	break;
      case 4:
	ostream << (uint32_t)integer;
	break;
      case 5:
	ostream << (uint64_t)integer;
	break;
      case 6:
	ostream << (uint64_t)integer;
	break;
      case 7:
	ostream << (uint64_t)integer;
	break;
      case 8:
	ostream << (uint64_t)integer;
	break;
      }
    }
    return ostream;
  }

  DER::Integer::~Integer()
  {
    if (this->_array != nullptr) delete[] this->_array;
  }

  void DER::encode_length(binarystream& stream, uint64_t length)
  {
    if (length < DER::POW_2_7) {
      stream.put((uint8_t)length);
    } else {
      if (length < DER::POW_2_8) {
	stream.put(0x81);
	stream.put((uint8_t)length);
      } else if (length < DER::POW_2_16) {
	stream.put(0x82);
	stream.put((uint8_t)(length >> 8));	
	stream.put((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_24) {
	stream.put(0x83);
	stream.put((uint8_t)(length >> 16));	
	stream.put((uint8_t)(length >> 8));	
	stream.put((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_32) {
	stream.put(0x84);
	stream.put((uint8_t)(length >> 24));	
	stream.put((uint8_t)(length >> 16));	
	stream.put((uint8_t)(length >> 8));	
	stream.put((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_40) {
	stream.put(0x85);
	stream.put((uint8_t)(length >> 32));	
	stream.put((uint8_t)(length >> 24));	
	stream.put((uint8_t)(length >> 16));	
	stream.put((uint8_t)(length >> 8));	
	stream.put((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_48) {
	stream.put(0x86);
	stream.put((uint8_t)(length >> 40));	
	stream.put((uint8_t)(length >> 32));	
	stream.put((uint8_t)(length >> 24));	
	stream.put((uint8_t)(length >> 16));	
	stream.put((uint8_t)(length >> 8));	
	stream.put((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_56) {
	stream.put(0x87);
	stream.put((uint8_t)(length >> 48));	
	stream.put((uint8_t)(length >> 40));	
	stream.put((uint8_t)(length >> 32));	
	stream.put((uint8_t)(length >> 24));	
	stream.put((uint8_t)(length >> 16));	
	stream.put((uint8_t)(length >> 8));	
	stream.put((uint8_t)(length & 0xFF));
      } else {
	stream.put(0x88);
	stream.put((uint8_t)(length >> 56));	
	stream.put((uint8_t)(length >> 48));	
	stream.put((uint8_t)(length >> 40));	
	stream.put((uint8_t)(length >> 32));	
	stream.put((uint8_t)(length >> 24));	
	stream.put((uint8_t)(length >> 16));	
	stream.put((uint8_t)(length >> 8));	
	stream.put((uint8_t)(length & 0xFF));
      }
    }
  }

  void DER::encode_boolean(binarystream& stream, bool value)
  {
    stream.put((uint8_t)DER::BOOLEAN);
    stream.put(0x01);
    if (value) {
      stream.put(0xFF);
    } else {
      stream.put(0x00);
    }
  }

  void DER::encode_integer(binarystream& stream, int8_t value)
  {
    stream.put((uint8_t)DER::INTEGER);
    stream.put(0x01);
    stream.put((uint8_t)value);
  }

  void DER::encode_integer(binarystream& stream, uint8_t value)
  {
    if (value < DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x02);
      stream.put(0x00);
      stream.put(value);
    }
  }

  void DER::encode_integer(binarystream& stream, int16_t ivalue)
  {
    uint16_t value = ivalue;
    if (value < (int64_t)DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else if (value < (int64_t)DER::POW_2_8) {
      DER::encode_integer(stream, (uint8_t)value);
    } else {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x02);
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(binarystream& stream, uint16_t value)
  {
    if (value < DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else if (value < DER::POW_2_8) {
      DER::encode_integer(stream, (uint8_t)value);
    } else if (value < DER::POW_2_15) {
      DER::encode_integer(stream, (int16_t)value);
    } else {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x03);
      stream.put(0x00);
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(binarystream& stream, int32_t ivalue)
  {
    uint32_t value = ivalue;
    if (value < (int64_t)DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else if (value < (int64_t)DER::POW_2_8) {
      DER::encode_integer(stream, (uint8_t)value);
    } else if (value < (int64_t)DER::POW_2_15) {
      DER::encode_integer(stream, (int16_t)value);
    } else if (value < (int64_t)DER::POW_2_16) {
      DER::encode_integer(stream, (uint16_t)value);
    } else if (value < (int64_t)DER::POW_2_23) {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x03);
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_24) {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x04);
      stream.put(0x00);
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    } else {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x04);
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(binarystream& stream, uint32_t value)
  {
    if (value < DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else if (value < DER::POW_2_8) {
      DER::encode_integer(stream, (uint8_t)value);
    } else if (value < DER::POW_2_15) {
      DER::encode_integer(stream, (int16_t)value);
    } else if (value < DER::POW_2_16) {
      DER::encode_integer(stream, (uint16_t)value);
    } else if (value < DER::POW_2_31) {
      DER::encode_integer(stream, (int32_t)value);
    } else {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x05);
      stream.put(0x00);
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(binarystream& stream, int64_t ivalue)
  {
    uint64_t value = ivalue;
    if (value < (int64_t)DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else if (value < (int64_t)DER::POW_2_8) {
      DER::encode_integer(stream, (uint8_t)value);
    } else if (value < (int64_t)DER::POW_2_15) {
      DER::encode_integer(stream, (int16_t)value);
    } else if (value < (int64_t)DER::POW_2_16) {
      DER::encode_integer(stream, (uint16_t)value);
    } else if (value < (int64_t)DER::POW_2_31) {
      DER::encode_integer(stream, (int32_t)value);
    } else if (value < (int64_t)DER::POW_2_32) {
      DER::encode_integer(stream, (uint32_t)value);
    } else if (value < (int64_t)DER::POW_2_39) {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x05);
      stream.put((uint8_t)(value >> 32));
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_40) {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x06);
      stream.put(0x00);
      stream.put((uint8_t)(value >> 32));
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_47) {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x06);
      stream.put((uint8_t)(value >> 40));
      stream.put((uint8_t)(value >> 32));
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_48) {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x07);
      stream.put(0x00);
      stream.put((uint8_t)(value >> 40));
      stream.put((uint8_t)(value >> 32));
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_55) {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x07);
      stream.put((uint8_t)(value >> 48));
      stream.put((uint8_t)(value >> 40));
      stream.put((uint8_t)(value >> 32));
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_56) {
      stream.put(0x08);
      stream.put(0x00);
      stream.put((uint8_t)(value >> 48));
      stream.put((uint8_t)(value >> 40));
      stream.put((uint8_t)(value >> 32));
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    } else {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x08);
      stream.put((uint8_t)(value >> 56));
      stream.put((uint8_t)(value >> 48));
      stream.put((uint8_t)(value >> 40));
      stream.put((uint8_t)(value >> 32));
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(binarystream& stream, uint64_t value)
  {
    if (value < DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else if (value < DER::POW_2_8) {
      DER::encode_integer(stream, (uint8_t)value);
    } else if (value < DER::POW_2_15) {
      DER::encode_integer(stream, (int16_t)value);
    } else if (value < DER::POW_2_16) {
      DER::encode_integer(stream, (uint16_t)value);
    } else if (value < DER::POW_2_31) {
      DER::encode_integer(stream, (int32_t)value);
    } else if (value < DER::POW_2_32) {
      DER::encode_integer(stream, (uint32_t)value);
    } else if (value < DER::POW_2_63) {
      DER::encode_integer(stream, (int64_t)value);
    } else {
      stream.put((uint8_t)DER::INTEGER);
      stream.put(0x09);
      stream.put(0x00);
      stream.put((uint8_t)(value >> 56));
      stream.put((uint8_t)(value >> 48));
      stream.put((uint8_t)(value >> 40));
      stream.put((uint8_t)(value >> 32));
      stream.put((uint8_t)(value >> 24));
      stream.put((uint8_t)(value >> 16));
      stream.put((uint8_t)(value >> 8));
      stream.put((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(binarystream& stream, const uint8_t* array, size_t length)
  {
    stream.put((uint8_t)DER::INTEGER);
    DER::encode_length(stream, length);
    stream.write((char*)array, length);    
  }

  void DER::encode_integer(binarystream& stream, binarystream& integer)
  {
    // NOTE: BigInt hands over whole machine words, so this emits a word padded
    // INTEGER (12345 becomes 00 00 30 39) rather than the minimal encoding
    // X.690 8.3.2 requires. DER::decode_integer expects the same padding, so
    // the pair round trips; it is only the on-the-wire form that other X.509
    // implementations reject. Trimming here alone breaks RSA key decoding --
    // the decoder has to learn arbitrary lengths at the same time.
    stream.put((uint8_t)DER::INTEGER);
    size_t length = integer.length();
    DER::encode_length(stream, length);
    integer.read(stream, 0, length);
  }

  void DER::encode_integer(binarystream& stream, const BigInt& number)
  {
    bufferstream bigint_stream;
    number.to_octet_string(bigint_stream, true);
    DER::encode_integer(stream, bigint_stream);
  }

  void DER::encode_bit_string(binarystream& stream, const uint8_t* array, size_t length)
  {
    stream.put((uint8_t)DER::BIT_STRING);
    size_t octet_length = (length % 8 == 0) ? (length / 8) + 1 : (length / 8) + 2;
    DER::encode_length(stream, octet_length);
    stream.put((uint8_t)length % 8);
    stream.write((char*)array, octet_length - 1);
  }

  void DER::encode_bit_string(binarystream& stream, binarystream& bit_string_stream, size_t length)
  {
    stream.put((uint8_t)DER::BIT_STRING);
    size_t octet_length = (length % 8 == 0) ? (length / 8) + 1 : (length / 8) + 2;
    DER::encode_length(stream, octet_length);
    stream.put((uint8_t)length % 8);
    bit_string_stream.read(stream, 0, octet_length);
  }

  void DER::encode_octet_string(binarystream& stream, const uint8_t* array, size_t length)
  {
    stream.put((uint8_t)DER::OCTET_STRING);
    DER::encode_length(stream, length);
    stream.write((char*)array, length);
  }

  void DER::encode_octet_string(binarystream& stream, binarystream& octet_string_stream)
  {
    stream.put((uint8_t)DER::OCTET_STRING);
    size_t length = octet_string_stream.length();
    DER::encode_length(stream, length);
    octet_string_stream.read(stream, 0, length);
  }

  void DER::encode_null(binarystream& stream)
  {
    stream.put((uint8_t)DER::NULL_VALUE);
    stream.put(0x00);
  }

  void DER::encode_oid_node(binarystream& stream, uint32_t node) {
    if (node < DER::POW_2_7) {
      stream.put((uint8_t)node);	
    } else if (node < DER::POW_2_8) {
      stream.put(0x81);	
      stream.put((uint8_t)node & 0x7F);	
    } else if (node < DER::POW_2_16) {
      if (node >> 14 > 0) {
	stream.put(0x80 | (uint8_t)(node >> 14));		  
	stream.put(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.put((uint8_t)node & 0x7F);
      } else {
	stream.put(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.put((uint8_t)node & 0x7F);
      }
    } else if (node < DER::POW_2_24) {
      if (node >> 21 > 0) {
	stream.put(0x80 | (uint8_t)(node >> 21));		  
	stream.put(0x80 | (uint8_t)((node >> 14) & 0x7F));		  
	stream.put(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.put((uint8_t)node & 0x7F);
      } else {
	stream.put(0x80 | (uint8_t)((node >> 14) & 0x7F));		  
	stream.put(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.put((uint8_t)node & 0x7F);
      }
    } else {
      if (node >> 28 > 0) {
	stream.put(0x80 | (uint8_t)(node >> 28));		  
	stream.put(0x80 | (uint8_t)((node >> 21) & 0x7F));		  
	stream.put(0x80 | (uint8_t)((node >> 14) & 0x7F));		  
	stream.put(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.put((uint8_t)node & 0x7F);
      } else {
	stream.put(0x80 | (uint8_t)((node >> 21) & 0x7F));		  
	stream.put(0x80 | (uint8_t)((node >> 14) & 0x7F));		  
	stream.put(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.put((uint8_t)node & 0x7F);
      }	
    }
  }

  void DER::encode_oid(binarystream& stream, uint32_t* nodes, size_t length)
  {
    stream.put((uint8_t)DER::OID);
    bufferstream nodes_stream;
    DER::encode_oid_node(nodes_stream, (*nodes * 40) + *(nodes + 1));
    for (size_t i = 2; i < length; i++) {
      DER::encode_oid_node(nodes_stream, *(nodes + i));
    }
    size_t octets_length = nodes_stream.length();
    DER::encode_length(stream, octets_length);
    nodes_stream.read(stream, 0, octets_length);
  }

  void DER::encode_oid(binarystream& stream, DER::oid_t nodes)
  {
    stream.put((uint8_t)DER::OID);
    bufferstream nodes_stream;
    DER::encode_oid_node(nodes_stream, (*(nodes.begin()) * 40) + *(nodes.begin() + 1));
    for (auto iter = nodes.begin() + 2; iter < nodes.end(); iter++) {
      DER::encode_oid_node(nodes_stream, *iter);
    }
    size_t octets_length = nodes_stream.length();
    DER::encode_length(stream, octets_length);
    nodes_stream.read(stream, 0, octets_length);
  }

  void DER::encode_utf8_string(binarystream& stream, const uint8_t* array, size_t length)
  {
    stream.put((uint8_t)DER::UTF8_STRING);
    DER::encode_length(stream, length);
    stream.write((char*)array, length);
  }

  void DER::encode_utf8_string(binarystream& stream, const std::string string)
  {
    DER::encode_utf8_string(stream, (uint8_t*)string.c_str(), string.length());
  }

  void DER::encode_printable_string(binarystream& stream, const char* array, size_t length)
  {
    static std::string allowed_chars = " '()+,-./:=?";
    stream.put((uint8_t)DER::PRINTABLE_STRING);
    DER::encode_length(stream, length);
    for (size_t i = 0; i < length; i++) {
      char ch = *array++;
      if (!Utility::in(ch, allowed_chars) && (ch < '0' || (ch > '9' && ch < 'A') || (ch > 'Z' && ch < 'a') || ch > 'z'))
	throw EncodingException("DER unallowed PrintableString character '" + std::string(&ch, 1) + "'");
      stream.put(ch);
    }
  }

  void DER::encode_printable_string(binarystream& stream, const std::string string)
  {
    DER::encode_printable_string(stream, string.c_str(), string.length());
  }

  void DER::encode_ia5_string(binarystream& stream, const char* array, size_t length)
  {
    stream.put((uint8_t)DER::IA5_STRING);
    DER::encode_length(stream, length);
    stream.write(array, length);
  }

  void DER::encode_ia5_string(binarystream& stream, const std::string input)
  {
    DER::encode_ia5_string(stream, input.c_str(), input.length());
  }

  void DER::encode_utc_time(binarystream& stream, time_t time, bool is_gmt)
  {
    stream.put((uint8_t)DER::UTC_TIME);
    DER::encode_length(stream, 13);
    std::string time_string = Utility::time_to_string(time, "%y%m%d%H%M%S", is_gmt);
    for (char c : time_string) stream.put(c);
    stream.put((is_gmt) ? (uint8_t)'Z' : (uint8_t)'0');
  }

  void DER::encode_generalized_time(binarystream& stream, time_t time, bool is_gmt)
  {
    stream.put((uint8_t)DER::GENERALIZED_TIME);
    DER::encode_length(stream, 15);
    std::string time_string = Utility::time_to_string(time, "%Y%m%d%H%M%S", is_gmt);
    for (char c : time_string) stream.put(c);
    stream.put((is_gmt) ? (uint8_t)'Z' : (uint8_t)'0');
  }

  void DER::encode_bmp_string(binarystream& stream, uint8_t* array, size_t length)
  {
    stream.put((uint8_t)DER::BMP_STRING);
    DER::encode_length(stream, length);
    stream.write((char*)array, length);
  }

  void DER::encode_sequence(binarystream& stream, binarystream& content_stream)
  {
    stream.put((uint8_t)DER::SEQUENCE);
    size_t length = content_stream.length();
    DER::encode_length(stream, length);
    content_stream.read(stream, 0, length);
  }

  void DER::encode_set(binarystream& stream, binarystream& content_stream)
  {
    stream.put((uint8_t)DER::SET);
    size_t length = content_stream.length();
    DER::encode_length(stream, length);
    content_stream.read(stream, 0, length);
  }

  uint8_t DER::decode_tag(binarystream& stream)
  {
    return stream.get();
  }

  uint64_t DER::decode_length(binarystream& stream)
  {
    uint8_t len_byte = stream.get();
    if ((len_byte & 0x80) == 0) {
      return len_byte;
    } else {
      len_byte &= 0x7F;
      uint64_t length = 0;
      for (size_t i = 0; i < len_byte; i++) {
	length <<= 8;
	length |= stream.get();
      }
      return length;
    }
  }

  bool DER::decode_boolean(binarystream& stream)
  {
    if (DER::decode_tag(stream) != DER::BOOLEAN) throw EncodingException("DER invalid TAG");
    if (DER::decode_length(stream) != 0x01) throw EncodingException("DER invalid LENGTH");
    switch (stream.get()) {
    case 0x00:
      return false;
    case 0xFF:
      return true;
    default:
      throw EncodingException("DER invalid VALUE");
    }
  }

  DER::Integer DER::decode_integer(binarystream& stream)
  {
    if (DER::decode_tag(stream) != DER::INTEGER) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    if (length == 0x00) throw EncodingException("DER invalid LENGTH");
    if (length > 1 && stream.peek() == 0) {
      Integer integer(length, false);
      for (size_t i = 0; i < length; i++) {
	*(integer._array + i) = stream.get();
      }
      return integer;
    } else {
      Integer integer(length, ((size_t)stream.peek() > POW_2_7) ? true : false);
      for (size_t i = 0; i < length; i++) {
	*(integer._array + i) = stream.get();
      }
      return integer;
    }
  }

  size_t DER::decode_bit_string(binarystream& stream, binarystream& bit_string)
  {
    if (DER::decode_tag(stream) != DER::BIT_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    uint8_t pad_len = stream.get();
    stream.read(bit_string, length - 1);
    return ((length - 1) * 8) - pad_len;
  }

  size_t DER::decode_octet_string(binarystream& stream, binarystream& octet_string)
  {
    if (DER::decode_tag(stream) != DER::OCTET_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    stream.read(octet_string, length);
    return length;
  }

  void DER::decode_null(binarystream& stream)
  {
    if (DER::decode_tag(stream) != DER::NULL_VALUE) throw EncodingException("DER invalid TAG");
    if (stream.get() != 0x00) throw EncodingException("DER invalid LENGTH");
  }

  uint32_t DER::decode_oid_node(binarystream& stream)
  {
    uint8_t octet = stream.get();
    if ((octet & 0x80) == 0) {
      return octet;
    } else {
      uint32_t node = 0;
      do {
	node <<= 7;
	node += (octet & 0x7F);
	octet = stream.get();
	if ((octet & 0x80) == 0) {
	  node <<= 7;
	  node += (octet & 0x7F);
	  break;
	}
      } while (true);
      return node;
    }
  }

  size_t DER::decode_oid(binarystream& stream, uint32_t* nodes)
  {
    if (DER::decode_tag(stream) != DER::OID) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    if (length == 0x00) throw EncodingException("DER invalid LENGTH");
    std::streampos end_stream = stream.tellg() + (std::streampos)length;
    *nodes++ = stream.peek() / 40;
    *nodes++ = stream.get() % 40;
    size_t nodes_len = 2;
    while (stream.tellg() < end_stream) {
      *nodes++ = DER::decode_oid_node(stream);
      nodes_len++;
    }
    return nodes_len;
  }

  DER::oid_t DER::decode_oid(binarystream& stream)
  {
    if (DER::decode_tag(stream) != DER::OID) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    if (length == 0x00) throw EncodingException("DER invalid LENGTH");
    std::streampos end_stream = stream.tellg() + (std::streampos)length;
    DER::oid_t nodes;
    nodes.push_back(stream.peek() / 40);
    nodes.push_back(stream.get() % 40);
    while (stream.tellg() < end_stream) {
      nodes.push_back(DER::decode_oid_node(stream));
    }
    return nodes;
  }

  size_t DER::decode_utf8_string(binarystream& stream, uint8_t* array)
  {
    if (DER::decode_tag(stream) != DER::UTF8_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    for (size_t i = 0; i < length; i++) {
      *array++ = stream.get();
    }
    return length;
  }

  size_t DER::decode_printable_string(binarystream& stream, char* array)
  {
    if (DER::decode_tag(stream) != DER::PRINTABLE_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    for (size_t i = 0; i < length; i++) {
      *array++ = stream.get();
    }
    return length;
  }

  size_t DER::decode_ia5_string(binarystream& stream, char* array)
  {
    if (DER::decode_tag(stream) != DER::IA5_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    for (size_t i = 0; i < length; i++) {
      *array++ = stream.get();
    }
    return length;
  }

  time_t DER::decode_utc_time(binarystream& stream)
  {
    if (DER::decode_tag(stream) != DER::UTC_TIME) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    if (length != 13) throw EncodingException("DER invalid LENGTH");
    char buffer[256];
    for (size_t i = 0; i < length; i++)
      buffer[i] = stream.get();
    bool is_gmt = (buffer[length - 1] == 'Z');
    buffer[length - 1] = '\0';
    return Utility::string_to_time(buffer, "%y%m%d%H%M%S", is_gmt);
  }

  time_t DER::decode_generalized_time(binarystream& stream)
  {
    if (DER::decode_tag(stream) != DER::GENERALIZED_TIME) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    if (length != 15) throw EncodingException("DER invalid LENGTH");
    char buffer[256];
    for (size_t i = 0; i < length; i++)
      buffer[i] = stream.get();
    bool is_gmt = (buffer[length - 1] == 'Z');
    buffer[length - 1] = '\0';
    return Utility::string_to_time(buffer, "%Y%m%d%H%M%S", is_gmt);
  }

  size_t DER::decode_bmp_string(binarystream& stream, uint8_t* array)
  {
    if (DER::decode_tag(stream) != DER::BMP_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    for (size_t i = 0; i < length; i++) {
      *array++ = stream.get();
    }
    return length;
  }

  void DER::decode_sequence(binarystream& stream, binarystream& content)
  {
    if (DER::decode_tag(stream) != DER::SEQUENCE) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    uint8_t buffer[length];
    stream.read((char*)buffer, length);
    content.write((char*)buffer, stream.gcount());
  }

  void DER::decode_set(binarystream& stream, binarystream& content)
  {
    if (DER::decode_tag(stream) != DER::SET) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(stream);
    uint8_t buffer[length];
    stream.read((char*)buffer, length);
    content.write((char*)buffer, stream.gcount());
  }

  void DER::decode_tlv(binarystream& stream, binarystream& tlv)
  {
    tlv.put(stream.get());
    size_t length = DER::decode_length(stream);
    DER::encode_length(tlv, length);
    uint8_t buffer[length];
    stream.read((char*)buffer, length);
    tlv.write((char*)buffer, length);
  }

  std::string DER::oid_to_string(oid_t& oid)
  {
    std::stringstream stream;
    stream << "{ ";
    for (uint32_t id : oid)
      stream << id << " ";
    stream << "}";
    return stream.str();
  }

}
