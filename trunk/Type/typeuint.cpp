#include "typeuint.h"
#include "typeint.h"
#include "typestring.h"
#include "binarystream.h"


namespace Zigurat
{

  const uint8_t UInt::TDB = TDByte::UINT;

  uint8_t UInt::tdb() const
  {
    return (this->_pointer == nullptr) ? UInt::TDB | TDByte::IS_NULL : UInt::TDB;
  }

  UInt::UInt()
  {
    this->_pointer = new uint32_t();
  }
  
  UInt::UInt(std::nullptr_t)
  {

  }

  UInt::UInt(uint32_t&& other)
  {
    this->_pointer = new uint32_t(std::move(other));
  }

  UInt::UInt(const uint32_t& other)
  {
    this->_pointer = new uint32_t(other);
  }

  UInt::UInt(UInt&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  UInt::UInt(const UInt& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new uint32_t(*(uint32_t*)other._pointer);
  }

  UInt::UInt(int&& other)
  {
    this->_pointer = new uint32_t(std::move(other));
  }
		
  UInt::UInt(const int& other)
  {
    this->_pointer = new uint32_t(other);
  }
	
  UInt::UInt(Int&& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint32_t(other.value());
  }
		
  UInt::UInt(const Int& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint32_t(other.value());
  }
	
  UInt& UInt::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  UInt& UInt::operator=(uint32_t&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new uint32_t(std::move(other));
    }
    return *this;
  }
	
  UInt& UInt::operator=(const uint32_t& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new uint32_t(other);
    }
    return *this;
  }

  UInt& UInt::operator=(UInt&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  UInt& UInt::operator=(const UInt& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new uint32_t(*(uint32_t*)other._pointer);
    }
    return *this;
  }

  bool UInt::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool UInt::operator==(uint32_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer == other);
  }
	
  bool UInt::operator==(const uint32_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer == other);
  }
	
  bool UInt::operator==(UInt&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer == *(uint32_t*)other._pointer);
  }
	
  bool UInt::operator==(const UInt& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer == *(uint32_t*)other._pointer);
  }
	
  bool UInt::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool UInt::operator!=(uint32_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer != other);
  }
	
  bool UInt::operator!=(const uint32_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer != other);
  }
	
  bool UInt::operator!=(UInt&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer != *(uint32_t*)other._pointer);
  }

  bool UInt::operator!=(const UInt& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer != *(uint32_t*)other._pointer);
  }
	
  bool UInt::operator<(const UInt& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer < *(uint32_t*)other._pointer);
  }
	
  bool UInt::operator<=(const UInt& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer <= *(uint32_t*)other._pointer);
  }
	
  bool UInt::operator>(const UInt& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer > *(uint32_t*)other._pointer);
  }
	
  bool UInt::operator>=(const UInt& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint32_t*)this->_pointer >= *(uint32_t*)other._pointer);
  }

  UInt UInt::operator+(UInt other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint32_t*)this->_pointer + *(uint32_t*)other._pointer;
  }
	
  UInt& UInt::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(uint32_t*)this->_pointer)++;
    return *this;
  }
	
  UInt& UInt::operator++(int)
  {
    this->UInt::operator++();
    return *this;
  }
	
  UInt UInt::operator-(UInt other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint32_t*)this->_pointer - *(uint32_t*)other._pointer;
  }
	
  UInt& UInt::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(uint32_t*)this->_pointer)--;
    return *this;
  }
	
  UInt UInt::operator*(UInt other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint32_t*)this->_pointer * *(uint32_t*)other._pointer;
  }
	
  UInt UInt::operator/(UInt other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint32_t*)this->_pointer / *(uint32_t*)other._pointer;
  }
	
  UInt UInt::operator%(UInt other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint32_t*)this->_pointer % *(uint32_t*)other._pointer;
  }
	
  void UInt::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (uint32_t*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const uint32_t& UInt::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint32_t*)this->_pointer;
  }

  std::string UInt::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(uint32_t*)this->_pointer);
  }

  UInt::~UInt()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, UInt&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_uint(*(uint32_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const UInt& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_uint(*(uint32_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, UInt& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new uint32_t();
      instream.read_std_uint(*(uint32_t*)object._pointer);
    }
    return instream;
  }

}
