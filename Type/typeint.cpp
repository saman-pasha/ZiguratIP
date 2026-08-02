#include "typeint.hpp"
#include "typestring.hpp"
#include "binarystream.hpp"


namespace Zigurat
{

  const uint8_t Int::TDB = TDByte::INT;

  uint8_t Int::tdb() const
  {
    return (this->_pointer == nullptr) ? Int::TDB | TDByte::IS_NULL : Int::TDB;
  }

  Int::Int()
  {
    this->_pointer = new int32_t();
  }
  
  Int::Int(std::nullptr_t)
  {

  }

  Int::Int(int32_t&& other)
  {
    this->_pointer = new int32_t(std::move(other));
  }

  Int::Int(const int32_t& other)
  {
    this->_pointer = new int32_t(other);
  }

  Int::Int(Int&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Int::Int(const Int& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new int32_t(*(int32_t*)other._pointer);
  }

  Int& Int::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Int& Int::operator=(int32_t&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new int32_t(std::move(other));
    }
    return *this;
  }
	
  Int& Int::operator=(const int32_t& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new int32_t(other);
    }
    return *this;
  }

  Int& Int::operator=(Int&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Int& Int::operator=(const Int& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new int32_t(*(int32_t*)other._pointer);
    }
    return *this;
  }

  bool Int::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Int::operator==(int32_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer == other);
  }
	
  bool Int::operator==(const int32_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer == other);
  }
	
  bool Int::operator==(Int&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer == *(int32_t*)other._pointer);
  }
	
  bool Int::operator==(const Int& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer == *(int32_t*)other._pointer);
  }
	
  bool Int::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Int::operator!=(int32_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer != other);
  }
	
  bool Int::operator!=(const int32_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer != other);
  }
	
  bool Int::operator!=(Int&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer != *(int32_t*)other._pointer);
  }

  bool Int::operator!=(const Int& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer != *(int32_t*)other._pointer);
  }
	
  bool Int::operator<(const Int& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer < *(int32_t*)other._pointer);
  }
	
  bool Int::operator<=(const Int& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer <= *(int32_t*)other._pointer);
  }
	
  bool Int::operator>(const Int& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer > *(int32_t*)other._pointer);
  }
	
  bool Int::operator>=(const Int& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int32_t*)this->_pointer >= *(int32_t*)other._pointer);
  }

  Int Int::operator+(Int other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int32_t*)this->_pointer + *(int32_t*)other._pointer;
  }
	
  Int& Int::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(int32_t*)this->_pointer)++;
    return *this;
  }
	
  Int& Int::operator++(int)
  {
    this->Int::operator++();
    return *this;
  }
	
  Int Int::operator-(Int other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int32_t*)this->_pointer - *(int32_t*)other._pointer;
  }
	
  Int& Int::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(int32_t*)this->_pointer)--;
    return *this;
  }
	
  Int Int::operator*(Int other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int32_t*)this->_pointer * *(int32_t*)other._pointer;
  }
	
  Int Int::operator/(Int other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int32_t*)this->_pointer / *(int32_t*)other._pointer;
  }
	
  Int Int::operator%(Int other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int32_t*)this->_pointer % *(int32_t*)other._pointer;
  }
	
  void Int::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (int32_t*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const int32_t& Int::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(int32_t*)this->_pointer;
  }

  std::string Int::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(int32_t*)this->_pointer);
  }

  Int::~Int()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Int&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_int(*(int32_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Int& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_int(*(int32_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Int& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new int32_t();
      instream.read_std_int(*(int32_t*)object._pointer);
    }
    return instream;
  }

}
