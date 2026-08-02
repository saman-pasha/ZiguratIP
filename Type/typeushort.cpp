#include "typeushort.h"
#include "typeint.h"
#include "typestring.h"
#include "binarystream.h"


namespace Zigurat
{

  const uint8_t UShort::TDB = TDByte::USHORT;

  uint8_t UShort::tdb() const
  {
    return (this->_pointer == nullptr) ? UShort::TDB | TDByte::IS_NULL : UShort::TDB;
  }

  UShort::UShort()
  {
    this->_pointer = new uint16_t();
  }
  
  UShort::UShort(std::nullptr_t)
  {

  }

  UShort::UShort(uint16_t&& other)
  {
    this->_pointer = new uint16_t(std::move(other));
  }

  UShort::UShort(const uint16_t& other)
  {
    this->_pointer = new uint16_t(other);
  }

  UShort::UShort(UShort&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  UShort::UShort(const UShort& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new uint16_t(*(uint16_t*)other._pointer);
  }

  UShort::UShort(int&& other)
  {
    this->_pointer = new uint16_t(std::move(other));
  }

  UShort::UShort(const int& other)
  {
    this->_pointer = new uint16_t(other);
  }
	
  UShort::UShort(Int&& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint16_t(other.value());
  }
		
  UShort::UShort(const Int& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint16_t(other.value());
  }
	
  UShort& UShort::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  UShort& UShort::operator=(uint16_t&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new uint16_t(std::move(other));
    }
    return *this;
  }
	
  UShort& UShort::operator=(const uint16_t& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new uint16_t(other);
    }
    return *this;
  }

  UShort& UShort::operator=(UShort&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  UShort& UShort::operator=(const UShort& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new uint16_t(*(uint16_t*)other._pointer);
    }
    return *this;
  }

  bool UShort::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool UShort::operator==(uint16_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer == other);
  }
	
  bool UShort::operator==(const uint16_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer == other);
  }
	
  bool UShort::operator==(UShort&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer == *(uint16_t*)other._pointer);
  }
	
  bool UShort::operator==(const UShort& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer == *(uint16_t*)other._pointer);
  }
	
  bool UShort::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool UShort::operator!=(uint16_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer != other);
  }
	
  bool UShort::operator!=(const uint16_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer != other);
  }
	
  bool UShort::operator!=(UShort&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer != *(uint16_t*)other._pointer);
  }

  bool UShort::operator!=(const UShort& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer != *(uint16_t*)other._pointer);
  }
	
  bool UShort::operator<(const UShort& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer < *(uint16_t*)other._pointer);
  }
	
  bool UShort::operator<=(const UShort& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer <= *(uint16_t*)other._pointer);
  }
	
  bool UShort::operator>(const UShort& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer > *(uint16_t*)other._pointer);
  }
	
  bool UShort::operator>=(const UShort& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint16_t*)this->_pointer >= *(uint16_t*)other._pointer);
  }

  UShort UShort::operator+(UShort other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint16_t*)this->_pointer + *(uint16_t*)other._pointer;
  }
	
  UShort& UShort::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(uint16_t*)this->_pointer)++;
    return *this;
  }
	
  UShort& UShort::operator++(int)
  {
    this->UShort::operator++();
    return *this;
  }
	
  UShort UShort::operator-(UShort other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint16_t*)this->_pointer - *(uint16_t*)other._pointer;
  }
	
  UShort& UShort::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(uint16_t*)this->_pointer)--;
    return *this;
  }
	
  UShort UShort::operator*(UShort other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint16_t*)this->_pointer * *(uint16_t*)other._pointer;
  }
	
  UShort UShort::operator/(UShort other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint16_t*)this->_pointer / *(uint16_t*)other._pointer;
  }
	
  UShort UShort::operator%(UShort other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint16_t*)this->_pointer % *(uint16_t*)other._pointer;
  }
  
  void UShort::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (uint16_t*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const uint16_t& UShort::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint16_t*)this->_pointer;
  }

  std::string UShort::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(uint16_t*)this->_pointer);
  }

  UShort::~UShort()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, UShort&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_ushort(*(uint16_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const UShort& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_ushort(*(uint16_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, UShort& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new uint16_t();
      instream.read_std_ushort(*(uint16_t*)object._pointer);
    }
    return instream;
  }

}
