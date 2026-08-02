#include "typebyte.hpp"
#include "typeint.hpp"
#include "typestring.hpp"
#include "binarystream.hpp"


namespace Zigurat
{

  const uint8_t Byte::TDB = TDByte::BYTE;

  uint8_t Byte::tdb() const
  {
    return (this->_pointer == nullptr) ? Byte::TDB | TDByte::IS_NULL : Byte::TDB;
  }

  Byte::Byte()
  {
    this->_pointer = new int8_t();
  }
  
  Byte::Byte(std::nullptr_t)
  {

  }

  Byte::Byte(int8_t&& other)
  {
    this->_pointer = new int8_t(std::move(other));
  }

  Byte::Byte(const int8_t& other)
  {
    this->_pointer = new int8_t(other);
  }

  Byte::Byte(Byte&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Byte::Byte(const Byte& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new int8_t(*(int8_t*)other._pointer);
  }

  Byte::Byte(int&& other)
  {
    this->_pointer = new int8_t(std::move(other));
  }

  Byte::Byte(const int& other)
  {
    this->_pointer = new int8_t(other);
  }
	
  Byte::Byte(Int&& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new int8_t(other.value());
  }

  Byte::Byte(const Int& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new int8_t(other.value());
  }
	
  Byte& Byte::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Byte& Byte::operator=(int8_t&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new int8_t(std::move(other));
    }
    return *this;
  }
	
  Byte& Byte::operator=(const int8_t& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new int8_t(other);
    }
    return *this;
  }

  Byte& Byte::operator=(Byte&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Byte& Byte::operator=(const Byte& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new int8_t(*(int8_t*)other._pointer);
    }
    return *this;
  }

  bool Byte::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Byte::operator==(int8_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer == other);
  }
	
  bool Byte::operator==(const int8_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer == other);
  }
	
  bool Byte::operator==(Byte&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer == *(int8_t*)other._pointer);
  }
	
  bool Byte::operator==(const Byte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer == *(int8_t*)other._pointer);
  }
	
  bool Byte::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Byte::operator!=(int8_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer != other);
  }
	
  bool Byte::operator!=(const int8_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer != other);
  }
	
  bool Byte::operator!=(Byte&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer != *(int8_t*)other._pointer);
  }

  bool Byte::operator!=(const Byte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer != *(int8_t*)other._pointer);
  }
	
  bool Byte::operator<(const Byte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer < *(int8_t*)other._pointer);
  }
	
  bool Byte::operator<=(const Byte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer <= *(int8_t*)other._pointer);
  }
	
  bool Byte::operator>(const Byte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer > *(int8_t*)other._pointer);
  }
	
  bool Byte::operator>=(const Byte& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int8_t*)this->_pointer >= *(int8_t*)other._pointer);
  }

  Byte Byte::operator+(Byte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int8_t*)this->_pointer + *(int8_t*)other._pointer;
  }
	
  Byte& Byte::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(int8_t*)this->_pointer)++;
    return *this;
  }
	
  Byte& Byte::operator++(int)
  {
    this->Byte::operator++();
    return *this;
  }
	
  Byte Byte::operator-(Byte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int8_t*)this->_pointer - *(int8_t*)other._pointer;
  }
	
  Byte& Byte::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(int8_t*)this->_pointer)--;
    return *this;
  }
	
  Byte Byte::operator*(Byte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int8_t*)this->_pointer * *(int8_t*)other._pointer;
  }
	
  Byte Byte::operator/(Byte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int8_t*)this->_pointer / *(int8_t*)other._pointer;
  }
	
  Byte Byte::operator%(Byte other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int8_t*)this->_pointer % *(int8_t*)other._pointer;
  }
	
  void Byte::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (int8_t*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const int8_t& Byte::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(int8_t*)this->_pointer;
  }

  std::string Byte::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(int8_t*)this->_pointer);
  }

  Byte::~Byte()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Byte&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_byte(*(int8_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Byte& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_byte(*(int8_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Byte& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new int8_t();
      instream.read_std_byte(*(int8_t*)object._pointer);
    }
    return instream;
  }
  
}
