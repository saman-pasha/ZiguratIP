#include "typeulong.hpp"
#include "typeint.hpp"
#include "typeuint.hpp"
#include "typestring.hpp"
#include "binarystream.hpp"


namespace Zigurat
{

  const uint8_t ULong::TDB = TDByte::ULONG;

  uint8_t ULong::tdb() const
  {
    return (this->_pointer == nullptr) ? ULong::TDB | TDByte::IS_NULL : ULong::TDB;
  }

  ULong::ULong()
  {
    this->_pointer = new uint64_t();
  }
  
  ULong::ULong(std::nullptr_t)
  {

  }

  ULong::ULong(uint64_t&& other)
  {
    this->_pointer = new uint64_t(std::move(other));
  }

  ULong::ULong(const uint64_t& other)
  {
    this->_pointer = new uint64_t(other);
  }

  ULong::ULong(ULong&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  ULong::ULong(const ULong& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new uint64_t(*(uint64_t*)other._pointer);
  }

  ULong::ULong(int&& other)
  {
    this->_pointer = new uint64_t(std::move(other));
  }

  ULong::ULong(const int& other)
  {
    this->_pointer = new uint64_t(other);
  }
	
  ULong::ULong(unsigned int&& other)
  {
    this->_pointer = new uint64_t(std::move(other));
  }

  ULong::ULong(const unsigned int& other)
  {
    this->_pointer = new uint64_t(other);
  }
	
  ULong::ULong(Int&& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint64_t(other.value());
  }

  ULong::ULong(const Int& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint64_t(other.value());
  }
	
  ULong::ULong(UInt&& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint64_t(other.value());
  }

  ULong::ULong(const UInt& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new uint64_t(other.value());
  }
	
  ULong& ULong::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  ULong& ULong::operator=(uint64_t&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new uint64_t(std::move(other));
    }
    return *this;
  }
	
  ULong& ULong::operator=(const uint64_t& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new uint64_t(other);
    }
    return *this;
  }

  ULong& ULong::operator=(ULong&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  ULong& ULong::operator=(const ULong& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new uint64_t(*(uint64_t*)other._pointer);
    }
    return *this;
  }

  bool ULong::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool ULong::operator==(uint64_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer == other);
  }
	
  bool ULong::operator==(const uint64_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer == other);
  }
	
  bool ULong::operator==(ULong&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer == *(uint64_t*)other._pointer);
  }
	
  bool ULong::operator==(const ULong& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer == *(uint64_t*)other._pointer);
  }
	
  bool ULong::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool ULong::operator!=(uint64_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer != other);
  }
	
  bool ULong::operator!=(const uint64_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer != other);
  }
	
  bool ULong::operator!=(ULong&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer != *(uint64_t*)other._pointer);
  }

  bool ULong::operator!=(const ULong& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer != *(uint64_t*)other._pointer);
  }
	
  bool ULong::operator<(const ULong& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer < *(uint64_t*)other._pointer);
  }
	
  bool ULong::operator<=(const ULong& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer <= *(uint64_t*)other._pointer);
  }
	
  bool ULong::operator>(const ULong& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer > *(uint64_t*)other._pointer);
  }
	
  bool ULong::operator>=(const ULong& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(uint64_t*)this->_pointer >= *(uint64_t*)other._pointer);
  }

  ULong ULong::operator+(ULong other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint64_t*)this->_pointer + *(uint64_t*)other._pointer;
  }
	
  ULong& ULong::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(uint64_t*)this->_pointer)++;
    return *this;
  }
	
  ULong& ULong::operator++(int)
  {
    this->ULong::operator++();
    return *this;
  }
	
  ULong ULong::operator-(ULong other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint64_t*)this->_pointer - *(uint64_t*)other._pointer;
  }
	
  ULong& ULong::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(uint64_t*)this->_pointer)--;
    return *this;
  }
	
  ULong ULong::operator*(ULong other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint64_t*)this->_pointer * *(uint64_t*)other._pointer;
  }
	
  ULong ULong::operator/(ULong other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint64_t*)this->_pointer / *(uint64_t*)other._pointer;
  }
	
  ULong ULong::operator%(ULong other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint64_t*)this->_pointer % *(uint64_t*)other._pointer;
  }
	
  void ULong::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (uint64_t*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const uint64_t& ULong::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(uint64_t*)this->_pointer;
  }

  std::string ULong::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(uint64_t*)this->_pointer);
  }

  ULong::~ULong()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, ULong&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_ulong(*(uint64_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const ULong& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_ulong(*(uint64_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, ULong& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new uint64_t();
      instream.read_std_ulong(*(uint64_t*)object._pointer);
    }
    return instream;
  }

}
