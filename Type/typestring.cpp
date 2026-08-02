#include "typestring.h"
#include "typebool.h"
#include "typechar.h"
#include "typebyte.h"
#include "typeubyte.h"
#include "typeshort.h"
#include "typeushort.h"
#include "typeint.h"
#include "typeuint.h"
#include "typelong.h"
#include "typeulong.h"
#include "typefloat.h"
#include "typedouble.h"
#include "typereal.h"
#include "typetimestamp.h"
#include "typetext.h"
#include <sstream>
#include "binarystream.h"


namespace Zigurat
{

  const uint8_t String::TDB = TDByte::STRING;

  uint8_t String::tdb() const
  {
    return (this->_pointer == nullptr) ? String::TDB | TDByte::IS_NULL : String::TDB;
  }

  String::String()
  {
    this->_pointer = new std::string();
  }
  
  String::String(std::nullptr_t)
  {

  }

  String::String(std::string&& other)
  {
    this->_pointer = new std::string(std::move(other));
  }

  String::String(const std::string& other)
  {
    this->_pointer = new std::string(other);
  }

  String::String(String&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  String::String(const String& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new std::string(*(std::string*)other._pointer);
  }

  String::String(const char* buffer)
  {
    this->_pointer = new std::string(buffer);
  }

  String::String(const char* buffer, size_t length)
  {
    this->_pointer = new std::string(buffer, length);
  }

  String& String::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  String& String::operator=(std::string&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new std::string(std::move(other));
    }
    return *this;
  }
	
  String& String::operator=(const std::string& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new std::string(other);
    }
    return *this;
  }

  String& String::operator=(String&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  String& String::operator=(const String& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new std::string(*(std::string*)other._pointer);
    }
    return *this;
  }

  String& String::operator=(const char* buffer)
  {
    this->set_null();
    this->_pointer = new std::string(buffer);
    return *this;
  }

  bool String::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool String::operator==(std::string&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer == other);
  }
	
  bool String::operator==(const std::string& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer == other);
  }
	
  bool String::operator==(String&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer == *(std::string*)other._pointer);
  }
	
  bool String::operator==(const String& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer == *(std::string*)other._pointer);
  }
	
  bool String::operator==(const char* other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer == std::string(other));
  }
	
  bool String::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool String::operator!=(std::string&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer != other);
  }
	
  bool String::operator!=(const std::string& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer != other);
  }
	
  bool String::operator!=(String&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer != *(std::string*)other._pointer);
  }

  bool String::operator!=(const String& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer != *(std::string*)other._pointer);
  }
	
  bool String::operator!=(const char* other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer != std::string(other));
  }
	
  bool String::operator<(const String& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer < *(std::string*)other._pointer);
  }
	
  bool String::operator<=(const String& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer <= *(std::string*)other._pointer);
  }
	
  bool String::operator>(const String& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer > *(std::string*)other._pointer);
  }
	
  bool String::operator>=(const String& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer >= *(std::string*)other._pointer);
  }

  String String::operator+(const char* value) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(std::string*)this->_pointer + std::string(value);
  }

  String operator+(const char* lhs, String rhs)
  {
    return std::string(lhs) + rhs.value();
  }

  String String::operator+(String other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(std::string*)this->_pointer + *(std::string*)other._pointer;
  }

  void String::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (std::string*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const std::string& String::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(std::string*)this->_pointer;
  }

  size_t String::std_size() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return ((std::string*)this->_pointer)->size();
  }
	
  ULong String::size() const
  {
    return (uint64_t)this->std_size();
  }
	
  ULong String::SIZE() const
  {
    return (uint64_t)this->std_size();
  }
	
  size_t String::std_length() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return ((std::string*)this->_pointer)->length();
  }

  ULong String::length() const
  {
    return (uint64_t)this->std_length();
  }

  ULong String::LENGTH() const
  {
    return (uint64_t)this->std_length();
  }

  Char String::GET(ULong index) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return Char((*(std::string*)this->_pointer)[index.value()]);
  }

  void String::SET(ULong index, const Char value)
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(std::string*)this->_pointer)[index.value()] = value.value();
  }

  Bool String::to_bool(bool alpha) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    bool value;
    std::stringstream ss(*(std::string*)this->_pointer);
    if (alpha)
      ss >> std::boolalpha >> value;
    else
      ss >> value;
    return Bool(value);
  }
	
  Char String::to_char() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return Char((*(std::string*)this->_pointer)[0]);
  }

  Byte String::to_byte() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    int value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return Byte(value);
  }
	
  UByte String::to_ubyte() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    int value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return UByte(value);
  }
	
  Short String::to_short() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    Short::SCT value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return Short(value);
  }
	
  UShort String::to_ushort() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    UShort::SCT value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return UShort(value);
  }
	
  Int String::to_int() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    Int::SCT value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return Int(value);
  }
	
  UInt String::to_uint() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    UInt::SCT value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return UInt(value);
  }
	
  Long String::to_long() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    Long::SCT value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return Long(value);
  }
	
  ULong String::to_ulong() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    ULong::SCT value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return ULong(value);
  }
	
  Float String::to_float() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    Float::SCT value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return Float(value);
  }
	
  Double String::to_double() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    Double::SCT value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return Double(value);
  }

  Real String::to_real() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    Real::SCT value;
    std::stringstream ss(*(std::string*)this->_pointer);
    ss >> value;
    return Real(value);
  }

  Timestamp String::to_timestamp(String format, Bool is_gmt) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    struct tm timeinfo;
    strptime(((std::string*)this->_pointer)->c_str(), format.value().c_str(), &timeinfo);
    return Timestamp((is_gmt) ? timegm(&timeinfo) : mktime(&timeinfo));
  }
	
  Text String::to_text() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return Text(*(std::string*)this->_pointer);
  }

  Bool String::TO_BOOL(Bool alpha) const
  {
    return this->to_bool(alpha);
  }
	
  Char String::TO_CHAR() const
  {
    return this->to_char();
  }

  Byte String::TO_BYTE() const
  {
    return this->to_byte();
  }
	
  UByte String::TO_UBYTE() const
  {
    return this->to_ubyte();
  }
	
  Short String::TO_SHORT() const
  {
    return this->to_short();
  }
	
  UShort String::TO_USHORT() const
  {
    return this->to_ushort();
  }
	
  Int String::TO_INT() const
  {
    return this->to_int();
  }
	
  UInt String::TO_UINT() const
  {
    return this->to_uint();
  }
	
  Long String::TO_LONG() const
  {
    return this->to_long();
  }
	
  ULong String::TO_ULONG() const
  {
    return this->to_ulong();
  }
	
  Float String::TO_FLOAT() const
  {
    return this->to_float();
  }
	
  Double String::TO_DOUBLE() const
  {
    return this->to_double();
  }

  Real String::TO_REAL() const
  {
    return this->to_real();
  }

  Timestamp String::TO_TIMESTAMP(String format, Bool is_gmt) const
  {
    return this->to_timestamp(format, is_gmt);
  }
	
  Text String::TO_TEXT() const
  {
    return this->to_text();
  }

  int64_t String::pack_size() const
  {
    return (this->_pointer == nullptr) ? 1 : 
      TDByte::SIZEOF_BYTE + (((std::string*)this->_pointer)->size() * TDByte::SIZEOF(Char::TDB)) + 1;
  }

  std::string String::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return *(std::string*)this->_pointer;
  }

  String::~String()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, String&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_string(*(std::string*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const String& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_string(*(std::string*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, String& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new std::string();
      instream.read_std_string(*(std::string*)object._pointer);
    }
    return instream;
  }

}
