#include "typeubyte.h"
#include "typeint.h"
#include "typestring.h"
#include "binarystream.h"


namespace Zigurat
{

  const uint8_t UByte::TDB = TDByte::UBYTE;

  uint8_t UByte::tdb() const
  {
    return (this->_pointer == nullptr) ? UByte::TDB | TDByte::IS_NULL : UByte::TDB;
  }

  UByte::UByte()
  {
    this->_pointer = new uint8_t();
  }
  
  UByte::UByte(std::nullptr_t)
  {

  }

  UByte::UByte(uint8_t&& other)
  {
    this->_pointer = new uint8_t(std::move(other));
  }

  UByte::UByte(const uint8_t& other)
  {
    this->_pointer = new uint8_t(other);
  }

  UByte::UByte(UByte&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  UByte::UByte(const UByte& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new uint8_t(*(uint8_t*)other._pointer);
  }

  UByte::UByte(int&& other)
  {
    this->_pointer = new uint8_t(std::move(other));
  }

  UByte::UByte(const int& other)
  {
    this->_pointer = new uint8_t(other);
  }
	
  UByte::UByte(Int&& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint8_t(other.value());
  }
		
  UByte::UByte(const Int& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint8_t(other.value());
  }

  UByte& UByte::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  UByte& UByte::operator=(uint8_t&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new uint8_t(std::move(other));
    }
    return *this;
  }
	
  UByte& UByte::operator=(const uint8_t& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new uint8_t(other);
    }
    return *this;
  }

  UByte& UByte::operator=(UByte&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  UByte& UByte::operator=(const UByte& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new uint8_t(*(uint8_t*)other._pointer);
    }
    return *this;
  }

  bool UByte::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool UByte::operator==(uint8_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer == other);
  }
	
  bool UByte::operator==(const uint8_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer == other);
  }
	
  bool UByte::operator==(UByte&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer == *(uint8_t*)other._pointer);
  }
	
  bool UByte::operator==(const UByte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer == *(uint8_t*)other._pointer);
  }
	
  bool UByte::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool UByte::operator!=(uint8_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer != other);
  }
	
  bool UByte::operator!=(const uint8_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer != other);
  }
	
  bool UByte::operator!=(UByte&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer != *(uint8_t*)other._pointer);
  }

  bool UByte::operator!=(const UByte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer != *(uint8_t*)other._pointer);
  }
	
  bool UByte::operator<(const UByte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer < *(uint8_t*)other._pointer);
  }
	
  bool UByte::operator<=(const UByte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer <= *(uint8_t*)other._pointer);
  }
	
  bool UByte::operator>(const UByte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer > *(uint8_t*)other._pointer);
  }
	
  bool UByte::operator>=(const UByte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint8_t*)this->_pointer >= *(uint8_t*)other._pointer);
  }

  UByte UByte::operator+(UByte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint8_t*)this->_pointer + *(uint8_t*)other._pointer;
  }
	
  UByte& UByte::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(uint8_t*)this->_pointer)++;
    return *this;
  }
	
  UByte& UByte::operator++(int)
  {
    this->UByte::operator++();
    return *this;
  }
	
  UByte UByte::operator-(UByte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint8_t*)this->_pointer - *(uint8_t*)other._pointer;
  }
	
  UByte& UByte::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(uint8_t*)this->_pointer)--;
    return *this;
  }
	
  UByte UByte::operator*(UByte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint8_t*)this->_pointer * *(uint8_t*)other._pointer;
  }
	
  UByte UByte::operator/(UByte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint8_t*)this->_pointer / *(uint8_t*)other._pointer;
  }
	
  UByte UByte::operator%(UByte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint8_t*)this->_pointer % *(uint8_t*)other._pointer;
  }
  
  void UByte::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (uint8_t*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const uint8_t& UByte::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint8_t*)this->_pointer;
  }

  std::string UByte::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(uint8_t*)this->_pointer);
  }

  UByte::~UByte()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, UByte&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_ubyte(*(uint8_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const UByte& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_ubyte(*(uint8_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, UByte& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new uint8_t();
      instream.read_std_ubyte(*(uint8_t*)object._pointer);
    }
    return instream;
  }
  
}
