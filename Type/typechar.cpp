#include "typechar.h"
#include "typestring.h"
#include "binarystream.h"


namespace Zigurat
{

  const uint8_t Char::TDB = TDByte::CHAR;

  uint8_t Char::tdb() const
  {
    return (this->_pointer == nullptr) ? Char::TDB | TDByte::IS_NULL : Char::TDB;
  }

  Char::Char()
  {
    this->_pointer = new char();
  }
  
  Char::Char(std::nullptr_t)
  {

  }

  Char::Char(char&& other)
  {
    this->_pointer = new char(std::move(other));
  }

  Char::Char(const char& other)
  {
    this->_pointer = new char(other);
  }

  Char::Char(Char&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Char::Char(const Char& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new char(*(char*)other._pointer);
  }

  Char::Char(const char* other)
  {
    this->_pointer = new char(*other);
  }
  
  Char& Char::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Char& Char::operator=(char&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new char(std::move(other));
    }
    return *this;
  }
	
  Char& Char::operator=(const char& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new char(other);
    }
    return *this;
  }

  Char& Char::operator=(Char&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Char& Char::operator=(const Char& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new char(*(char*)other._pointer);
    }
    return *this;
  }

  bool Char::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Char::operator==(char&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer == other);
  }
	
  bool Char::operator==(const char& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer == other);
  }
	
  bool Char::operator==(Char&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer == *(char*)other._pointer);
  }
	
  bool Char::operator==(const Char& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer == *(char*)other._pointer);
  }
	
  bool Char::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Char::operator!=(char&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer != other);
  }
	
  bool Char::operator!=(const char& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer != other);
  }
	
  bool Char::operator!=(Char&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer != *(char*)other._pointer);
  }

  bool Char::operator!=(const Char& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer != *(char*)other._pointer);
  }
	
  bool Char::operator<(const Char& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer < *(char*)other._pointer);
  }
	
  bool Char::operator<=(const Char& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer <= *(char*)other._pointer);
  }
	
  bool Char::operator>(const Char& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer > *(char*)other._pointer);
  }
	
  bool Char::operator>=(const Char& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(char*)this->_pointer >= *(char*)other._pointer);
  }

  void Char::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (char*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const char& Char::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(char*)this->_pointer;
  }

  std::string Char::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::string(1, *(char*)this->_pointer);
  }

  Char::~Char()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Char&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_char(*(char*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Char& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_char(*(char*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Char& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new char();
      instream.read_std_char(*(char*)object._pointer);
    }
    return instream;
  }

}
