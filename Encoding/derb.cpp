#include "der.hpp"
#include <cstring>
#include <algorithm>
#include "encodingexception.hpp"


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

  std::ostream& operator<<(std::ostream& ostream, const DER::Integer& integer)
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
    delete[] this->_array;
  }

  void DER::encode_length(stream_t& stream, size_t length)
  {
    if (length < DER::POW_2_7) {
      stream.push_back((uint8_t)length);
    } else {
      if (length < DER::POW_2_8) {
	stream.push_back(0x81);
	stream.push_back((uint8_t)length);
      } else if (length < DER::POW_2_16) {
	stream.push_back(0x82);
	stream.push_back((uint8_t)(length >> 8));	
	stream.push_back((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_24) {
	stream.push_back(0x83);
	stream.push_back((uint8_t)(length >> 16));	
	stream.push_back((uint8_t)(length >> 8));	
	stream.push_back((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_32) {
	stream.push_back(0x84);
	stream.push_back((uint8_t)(length >> 24));	
	stream.push_back((uint8_t)(length >> 16));	
	stream.push_back((uint8_t)(length >> 8));	
	stream.push_back((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_40) {
	stream.push_back(0x85);
	stream.push_back((uint8_t)(length >> 32));	
	stream.push_back((uint8_t)(length >> 24));	
	stream.push_back((uint8_t)(length >> 16));	
	stream.push_back((uint8_t)(length >> 8));	
	stream.push_back((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_48) {
	stream.push_back(0x86);
	stream.push_back((uint8_t)(length >> 40));	
	stream.push_back((uint8_t)(length >> 32));	
	stream.push_back((uint8_t)(length >> 24));	
	stream.push_back((uint8_t)(length >> 16));	
	stream.push_back((uint8_t)(length >> 8));	
	stream.push_back((uint8_t)(length & 0xFF));
      } else if (length < DER::POW_2_56) {
	stream.push_back(0x87);
	stream.push_back((uint8_t)(length >> 48));	
	stream.push_back((uint8_t)(length >> 40));	
	stream.push_back((uint8_t)(length >> 32));	
	stream.push_back((uint8_t)(length >> 24));	
	stream.push_back((uint8_t)(length >> 16));	
	stream.push_back((uint8_t)(length >> 8));	
	stream.push_back((uint8_t)(length & 0xFF));
      } else {
	stream.push_back(0x88);
	stream.push_back((uint8_t)(length >> 56));	
	stream.push_back((uint8_t)(length >> 48));	
	stream.push_back((uint8_t)(length >> 40));	
	stream.push_back((uint8_t)(length >> 32));	
	stream.push_back((uint8_t)(length >> 24));	
	stream.push_back((uint8_t)(length >> 16));	
	stream.push_back((uint8_t)(length >> 8));	
	stream.push_back((uint8_t)(length & 0xFF));
      }
    }
  }

  void DER::encode_boolean(stream_t& stream, bool value)
  {
    stream.push_back(DER::BOOLEAN);
    stream.push_back(0x01);
    if (value) {
      stream.push_back(0xFF);
    } else {
      stream.push_back(0x00);
    }
  }

  void DER::encode_integer(stream_t& stream, int8_t value)
  {
    stream.push_back(DER::INTEGER);
    stream.push_back(0x01);
    stream.push_back((uint8_t)value);
  }

  void DER::encode_integer(stream_t& stream, uint8_t value)
  {
    if (value < DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x02);
      stream.push_back(0x00);
      stream.push_back(value);
    }
  }

  void DER::encode_integer(stream_t& stream, int16_t ivalue)
  {
    uint16_t value = ivalue;
    if (value < (int64_t)DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else if (value < (int64_t)DER::POW_2_8) {
      DER::encode_integer(stream, (uint8_t)value);
    } else {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x02);
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(stream_t& stream, uint16_t value)
  {
    if (value < DER::POW_2_7) {
      DER::encode_integer(stream, (int8_t)value);
    } else if (value < DER::POW_2_8) {
      DER::encode_integer(stream, (uint8_t)value);
    } else if (value < DER::POW_2_15) {
      DER::encode_integer(stream, (int16_t)value);
    } else {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x03);
      stream.push_back(0x00);
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(stream_t& stream, int32_t ivalue)
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
      stream.push_back(DER::INTEGER);
      stream.push_back(0x03);
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_24) {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x04);
      stream.push_back(0x00);
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    } else {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x04);
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(stream_t& stream, uint32_t value)
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
      stream.push_back(DER::INTEGER);
      stream.push_back(0x05);
      stream.push_back(0x00);
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(stream_t& stream, int64_t ivalue)
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
      stream.push_back(DER::INTEGER);
      stream.push_back(0x05);
      stream.push_back((uint8_t)(value >> 32));
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_40) {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x06);
      stream.push_back(0x00);
      stream.push_back((uint8_t)(value >> 32));
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_47) {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x06);
      stream.push_back((uint8_t)(value >> 40));
      stream.push_back((uint8_t)(value >> 32));
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_48) {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x07);
      stream.push_back(0x00);
      stream.push_back((uint8_t)(value >> 40));
      stream.push_back((uint8_t)(value >> 32));
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_55) {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x07);
      stream.push_back((uint8_t)(value >> 48));
      stream.push_back((uint8_t)(value >> 40));
      stream.push_back((uint8_t)(value >> 32));
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    } else if (value < (int64_t)DER::POW_2_56) {
      stream.push_back(0x08);
      stream.push_back(0x00);
      stream.push_back((uint8_t)(value >> 48));
      stream.push_back((uint8_t)(value >> 40));
      stream.push_back((uint8_t)(value >> 32));
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    } else {
      stream.push_back(DER::INTEGER);
      stream.push_back(0x08);
      stream.push_back((uint8_t)(value >> 56));
      stream.push_back((uint8_t)(value >> 48));
      stream.push_back((uint8_t)(value >> 40));
      stream.push_back((uint8_t)(value >> 32));
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(stream_t& stream, uint64_t value)
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
      stream.push_back(DER::INTEGER);
      stream.push_back(0x09);
      stream.push_back(0x00);
      stream.push_back((uint8_t)(value >> 56));
      stream.push_back((uint8_t)(value >> 48));
      stream.push_back((uint8_t)(value >> 40));
      stream.push_back((uint8_t)(value >> 32));
      stream.push_back((uint8_t)(value >> 24));
      stream.push_back((uint8_t)(value >> 16));
      stream.push_back((uint8_t)(value >> 8));
      stream.push_back((uint8_t)(value & 0xFF));
    }
  }

  void DER::encode_integer(stream_t& stream, const uint8_t* array, size_t length)
  {
    stream.push_back(DER::INTEGER);
    DER::encode_length(stream, length);
    for (size_t i = 0; i < length; i++)
      stream.push_back(*array++);    
  }

  void DER::encode_bit_string(stream_t& stream, const uint8_t* array, size_t length)
  {
    stream.push_back(DER::BIT_STRING);
    size_t octet_length = (length % 8 == 0) ? length / 8 : (length / 8) + 1;
    DER::encode_length(stream, octet_length);
    stream.push_back((uint8_t)length % 8);
    for (size_t i = 0; i < octet_length; i++)
      stream.push_back(*array++);
  }

  void DER::encode_octet_string(stream_t& stream, const uint8_t* array, size_t length)
  {
    stream.push_back(DER::OCTET_STRING);
    DER::encode_length(stream, length);
    for (size_t i = 0; i < length; i++)
      stream.push_back(*array++);
  }

  void DER::encode_null(stream_t& stream)
  {
    stream.push_back(DER::NULL_VALUE);
    stream.push_back(0x00);
  }

  void DER::encode_oid_node(stream_t& stream, uint32_t node) {
    if (node < DER::POW_2_7) {
      stream.push_back((uint8_t)node);	
    } else if (node < DER::POW_2_8) {
      stream.push_back(0x81);	
      stream.push_back((uint8_t)node & 0x7F);	
    } else if (node < DER::POW_2_16) {
      if (node >> 14 > 0) {
	stream.push_back(0x80 | (uint8_t)(node >> 14));		  
	stream.push_back(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.push_back((uint8_t)node & 0x7F);
      } else {
	stream.push_back(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.push_back((uint8_t)node & 0x7F);
      }
    } else if (node < DER::POW_2_24) {
      if (node >> 21 > 0) {
	stream.push_back(0x80 | (uint8_t)(node >> 21));		  
	stream.push_back(0x80 | (uint8_t)((node >> 14) & 0x7F));		  
	stream.push_back(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.push_back((uint8_t)node & 0x7F);
      } else {
	stream.push_back(0x80 | (uint8_t)((node >> 14) & 0x7F));		  
	stream.push_back(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.push_back((uint8_t)node & 0x7F);
      }
    } else {
      if (node >> 28 > 0) {
	stream.push_back(0x80 | (uint8_t)(node >> 28));		  
	stream.push_back(0x80 | (uint8_t)((node >> 21) & 0x7F));		  
	stream.push_back(0x80 | (uint8_t)((node >> 14) & 0x7F));		  
	stream.push_back(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.push_back((uint8_t)node & 0x7F);
      } else {
	stream.push_back(0x80 | (uint8_t)((node >> 21) & 0x7F));		  
	stream.push_back(0x80 | (uint8_t)((node >> 14) & 0x7F));		  
	stream.push_back(0x80 | (uint8_t)((node >> 7) & 0x7F));		  
	stream.push_back((uint8_t)node & 0x7F);
      }	
    }
  }

  void DER::encode_oid(stream_t& stream, uint32_t* nodes, size_t length)
  {
    stream.push_back(DER::OID);
    stream_t nodes_stream;
    DER::encode_oid_node(nodes_stream, (*nodes * 40) + *(nodes + 1));
    for (size_t i = 2; i < length; i++) {
      DER::encode_oid_node(nodes_stream, *(nodes + i));
    }
    DER::encode_length(stream, nodes_stream.size());
    std::copy(nodes_stream.begin(), nodes_stream.end(), std::back_inserter(stream));
  }

  void DER::encode_oid(stream_t& stream, DER::oid_t nodes)
  {
    stream.push_back(DER::OID);
    stream_t nodes_stream;
    DER::encode_oid_node(nodes_stream, (*(nodes.begin()) * 40) + *(nodes.begin() + 1));
    for (auto iter = nodes.begin() + 2; iter < nodes.end(); iter++) {
      DER::encode_oid_node(nodes_stream, *iter);
    }
    DER::encode_length(stream, nodes_stream.size());
    std::copy(nodes_stream.begin(), nodes_stream.end(), std::back_inserter(stream));
  }

  void DER::encode_utf8_string(stream_t& stream, uint8_t* array, size_t length)
  {
    stream.push_back(DER::UTF8_STRING);
    DER::encode_length(stream, length);
    for (size_t i = 0; i < length; i++)
      stream.push_back(*array++);
  }

  void DER::encode_printable_string(stream_t& stream, uint8_t* array, size_t length)
  {
    stream.push_back(DER::PRINTABLE_STRING);
    DER::encode_length(stream, length);
    for (size_t i = 0; i < length; i++)
      stream.push_back(*array++);
  }

  void DER::encode_ia5_string(stream_t& stream, uint8_t* array, size_t length)
  {
    stream.push_back(DER::IA5_STRING);
    DER::encode_length(stream, length);
    for (size_t i = 0; i < length; i++)
      stream.push_back(*array++);
  }

  void DER::encode_utc_time(stream_t& stream, time_t time, bool is_gmt)
  {
    stream.push_back(DER::UTC_TIME);
    DER::encode_length(stream, 13);
    char buffer[256];
    struct tm* timeinfo = (is_gmt) ? gmtime(&time) : localtime(&time);
    size_t length = strftime(buffer, 255, "%y%m%d%H%M%S", timeinfo);
    //delete timeinfo;
    for (size_t i = 0; i < length; i++)
      stream.push_back(buffer[i]);
    stream.push_back((is_gmt) ? (uint8_t)'Z' : (uint8_t)'0');
  }

  void DER::encode_generalized_time(stream_t& stream, time_t time, bool is_gmt)
  {
    stream.push_back(DER::GENERALIZED_TIME);
    DER::encode_length(stream, 15);
    char buffer[256];
    struct tm* timeinfo = (is_gmt) ? gmtime(&time) : localtime(&time);
    size_t length = strftime(buffer, 255, "%Y%m%d%H%M%S", timeinfo);
    //delete timeinfo;
    for (size_t i = 0; i < length; i++)
      stream.push_back(buffer[i]);
    stream.push_back((is_gmt) ? (uint8_t)'Z' : (uint8_t)'0');
  }

  void DER::encode_bmp_string(stream_t& stream, uint8_t* array, size_t length)
  {
    stream.push_back(DER::BMP_STRING);
    DER::encode_length(stream, length);
    for (size_t i = 0; i < length; i++)
      stream.push_back(*array++);
  }

  void DER::encode_sequence(stream_t& stream, size_t length)
  {
    stream.push_back(DER::SEQUENCE);
    DER::encode_length(stream, length);
  }

  void DER::encode_set(stream_t& stream, size_t length)
  {
    stream.push_back(DER::SET);
    DER::encode_length(stream, length);
  }

  uint8_t DER::decode_tag(position_t& pos)
  {
    return *pos++;
  }

  size_t DER::decode_length(position_t& pos)
  {
    uint8_t len_byte = *pos++;
    if ((len_byte & 0x80) == 0) {
      return len_byte;
    } else {
      len_byte &= 0x7F;
      size_t length = 0;
      for (size_t i = 0; i < len_byte; i++) {
	length <<= 8;
	length |= *pos++;
      }
      return length;
    }
  }

  bool DER::decode_boolean(position_t& pos)
  {
    if (DER::decode_tag(pos) != DER::BOOLEAN) throw EncodingException("DER invalid TAG");
    if (DER::decode_length(pos) != 0x01) throw EncodingException("DER invalid LENGTH");
    switch (*pos++) {
    case 0x00:
      return false;
    case 0xFF:
      return true;
    default:
      throw EncodingException("DER invalid VALUE");
    }
  }

  DER::Integer DER::decode_integer(position_t& pos)
  {
    if (DER::decode_tag(pos) != DER::INTEGER) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    if (length == 0x00) throw EncodingException("DER invalid LENGTH");
    if (length > 1 && *pos == 0) {
      pos++;
      length = length - 1;
      Integer integer(length, false);
      for (size_t i = 0; i < length; i++) {
	*(integer._array + i) = *pos++;
      }
      return integer;
    } else {
      Integer integer(length, (*pos > POW_2_7) ? true : false);
      for (size_t i = 0; i < length; i++) {
	*(integer._array + i) = *pos++;
      }
      return integer;
    }
  }

  size_t DER::decode_bit_string(position_t& pos, uint8_t* array)
  {
    if (DER::decode_tag(pos) != DER::BIT_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    uint8_t pad_len = *pos++;
    for (size_t i = 1; i < length; i++) {
      *array++ = *pos++;
    }
    return (length * 8) - pad_len;
  }

  size_t DER::decode_octet_string(position_t& pos, uint8_t* array)
  {
    if (DER::decode_tag(pos) != DER::OCTET_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    for (size_t i = 0; i < length; i++) {
      *array++ = *pos++;
    }
    return length;
  }

  void DER::decode_null(position_t& pos)
  {
    if (DER::decode_tag(pos) != DER::NULL_VALUE) throw EncodingException("DER invalid TAG");
    if (*pos++ != 0x00) throw EncodingException("DER invalid LENGTH");
  }

  uint32_t DER::decode_oid_node(position_t& pos)
  {
    uint8_t octet = *pos++;
    if ((octet & 0x80) == 0) {
      return octet;
    } else {
      uint32_t node = 0;
      do {
	node <<= 7;
	node += (octet & 0x7F);
	octet = *pos++;
	if ((octet & 0x80) == 0) {
	  node <<= 7;
	  node += (octet & 0x7F);
	  break;
	}
      } while (true);
      return node;
    }
  }

  size_t DER::decode_oid(position_t& pos, uint32_t* nodes)
  {
    if (DER::decode_tag(pos) != DER::OID) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    if (length == 0x00) throw EncodingException("DER invalid LENGTH");
    position_t end_pos = pos + length;
    *nodes++ = *pos / 40;
    *nodes++ = *pos++ % 40;
    size_t nodes_len = 2;
    while (pos < end_pos) {
      *nodes++ = DER::decode_oid_node(pos);
      nodes_len++;
    }
    return nodes_len;
  }

  DER::oid_t DER::decode_oid(position_t& pos)
  {
    if (DER::decode_tag(pos) != DER::OID) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    if (length == 0x00) throw EncodingException("DER invalid LENGTH");
    position_t end_pos = pos + length;
    DER::oid_t nodes;
    nodes.push_back(*pos / 40);
    nodes.push_back(*pos++ % 40);
    while (pos < end_pos) {
      nodes.push_back(DER::decode_oid_node(pos));
    }
    return nodes;
  }

  size_t DER::decode_utf8_string(position_t& pos, uint8_t* array)
  {
    if (DER::decode_tag(pos) != DER::UTF8_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    for (size_t i = 0; i < length; i++) {
      *array++ = *pos++;
    }
    return length;
  }

  size_t DER::decode_printable_string(position_t& pos, uint8_t* array)
  {
    if (DER::decode_tag(pos) != DER::PRINTABLE_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    for (size_t i = 0; i < length; i++) {
      *array++ = *pos++;
    }
    return length;
  }

  size_t DER::decode_ia5_string(position_t& pos, uint8_t* array)
  {
    if (DER::decode_tag(pos) != DER::IA5_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    for (size_t i = 0; i < length; i++) {
      *array++ = *pos++;
    }
    return length;
  }

  time_t DER::decode_utc_time(position_t& pos)
  {
    if (DER::decode_tag(pos) != DER::UTC_TIME) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    if (length != 13) throw EncodingException("DER invalid LENGTH");
    char buffer[256];
    for (size_t i = 0; i < length; i++)
      buffer[i] = *pos++;
    bool is_gmt = (buffer[length - 1] == 'Z');
    buffer[length - 1] = '\0';
    struct tm timeinfo;
    strptime(buffer, "%y%m%d%H%M%S", &timeinfo);
    return (is_gmt) ? timegm(&timeinfo) : mktime(&timeinfo);
  }

  time_t DER::decode_generalized_time(position_t& pos)
  {
    if (DER::decode_tag(pos) != DER::GENERALIZED_TIME) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    if (length != 15) throw EncodingException("DER invalid LENGTH");
    char buffer[256];
    for (size_t i = 0; i < length; i++)
      buffer[i] = *pos++;
    bool is_gmt = (buffer[length - 1] == 'Z');
    buffer[length - 1] = '\0';
    struct tm timeinfo;
    strptime(buffer, "%Y%m%d%H%M%S", &timeinfo);
    return (is_gmt) ? timegm(&timeinfo) : mktime(&timeinfo);
  }

  size_t DER::decode_bmp_string(position_t& pos, uint8_t* array)
  {
    if (DER::decode_tag(pos) != DER::BMP_STRING) throw EncodingException("DER invalid TAG");
    size_t length = DER::decode_length(pos);
    for (size_t i = 0; i < length; i++) {
      *array++ = *pos++;
    }
    return length;
  }

  size_t DER::decode_sequence(position_t& pos)
  {
    if (DER::decode_tag(pos) != DER::SEQUENCE) throw EncodingException("DER invalid TAG");
    return DER::decode_length(pos);
  }

  size_t DER::decode_set(position_t& pos)
  {
    if (DER::decode_tag(pos) != DER::SET) throw EncodingException("DER invalid TAG");
    return DER::decode_length(pos);
  }

}
