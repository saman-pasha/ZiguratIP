#include "typetext.h"
#include "typechar.h"
#include "typestring.h"
#include "binarystream.h"


namespace Zigurat
{

  const uint8_t Text::TDB = TDByte::TEXT;

  uint8_t Text::tdb() const
  {
    return (this->_pointer == nullptr) ? Text::TDB | TDByte::IS_NULL : Text::TDB;
  }

  Text::Text()
  {
    this->_pointer = new std::string();
  }
  
  Text::Text(std::nullptr_t)
  {

  }

  Text::Text(std::string&& other)
  {
    this->_pointer = new std::string(std::move(other));
  }

  Text::Text(const std::string& other)
  {
    this->_pointer = new std::string(other);
  }

  Text::Text(Text&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Text::Text(const Text& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new std::string(*(std::string*)other._pointer);
  }

  Text::Text(const char* buffer)
  {
    this->_pointer = new std::string(buffer);
  }

  Text::Text(const char* buffer, size_t length)
  {
    this->_pointer = new std::string(buffer, length);
  }

  Text::Text(String&& other)
  {
    this->_pointer = new std::string(other.value());
  }

  Text::Text(const String& other)
  {
    this->_pointer = new std::string(other.value());
  }
	
  Text& Text::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Text& Text::operator=(std::string&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new std::string(std::move(other));
    }
    return *this;
  }
	
  Text& Text::operator=(const std::string& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new std::string(other);
    }
    return *this;
  }

  Text& Text::operator=(Text&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Text& Text::operator=(const Text& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new std::string(*(std::string*)other._pointer);
    }
    return *this;
  }

  Text& Text::operator=(const char* buffer)
  {
    this->set_null();
    this->_pointer = new std::string(buffer);
    return *this;
  }

  bool Text::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Text::operator==(std::string&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer == other);
  }
	
  bool Text::operator==(const std::string& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer == other);
  }
	
  bool Text::operator==(Text&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer == *(std::string*)other._pointer);
  }
	
  bool Text::operator==(const Text& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer == *(std::string*)other._pointer);
  }
	
  bool Text::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Text::operator!=(std::string&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer != other);
  }
	
  bool Text::operator!=(const std::string& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer != other);
  }
	
  bool Text::operator!=(Text&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer != *(std::string*)other._pointer);
  }

  bool Text::operator!=(const Text& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer != *(std::string*)other._pointer);
  }
	
  bool Text::operator<(const Text& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer < *(std::string*)other._pointer);
  }
	
  bool Text::operator<=(const Text& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer <= *(std::string*)other._pointer);
  }
	
  bool Text::operator>(const Text& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer > *(std::string*)other._pointer);
  }
	
  bool Text::operator>=(const Text& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::string*)this->_pointer >= *(std::string*)other._pointer);
  }

  Text Text::operator+(const char* value) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(std::string*)this->_pointer + std::string(value);
  }

  Text operator+(const char* lhs, Text rhs)
  {
    return std::string(lhs) + rhs.value();
  }

  Text Text::operator+(Text other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(std::string*)this->_pointer + *(std::string*)other._pointer;
  }

  void Text::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (std::string*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const std::string& Text::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(std::string*)this->_pointer;
  }

  size_t Text::std_size() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return ((std::string*)this->_pointer)->size();
  }
	
  ULong Text::size() const
  {
    return (uint64_t)this->std_size();
  }
	
  ULong Text::SIZE() const
  {
    return (uint64_t)this->std_size();
  }
	
  size_t Text::std_length() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return ((std::string*)this->_pointer)->length();
  }

  ULong Text::length() const
  {
    return (uint64_t)this->std_length();
  }

  ULong Text::LENGTH() const
  {
    return (uint64_t)this->std_length();
  }

  int64_t Text::pack_size() const
  {
    return (this->_pointer == nullptr) ? 1 : 
      TDByte::SIZEOF_WORD + (((std::string*)this->_pointer)->size() * TDByte::SIZEOF(Char::TDB)) + 1;
  }

  std::string Text::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return *(std::string*)this->_pointer;
  }

  Text::~Text()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Text&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_text(*(std::string*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Text& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_text(*(std::string*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Text& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new std::string();
      instream.read_std_text(*(std::string*)object._pointer);
    }
    return instream;
  }

}
