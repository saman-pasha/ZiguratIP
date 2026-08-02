#include "typefloat.h"
#include "typestring.h"
#include "binarystream.h"


namespace Zigurat
{

  const uint8_t Float::TDB = TDByte::FLOAT;

  uint8_t Float::tdb() const
  {
    return (this->_pointer == nullptr) ? Float::TDB | TDByte::IS_NULL : Float::TDB;
  }

  Float::Float()
  {
    this->_pointer = new float();
  }
  
  Float::Float(std::nullptr_t)
  {

  }

  Float::Float(float&& other)
  {
    this->_pointer = new float(std::move(other));
  }

  Float::Float(const float& other)
  {
    this->_pointer = new float(other);
  }

  Float::Float(Float&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Float::Float(const Float& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new float(*(float*)other._pointer);
  }

  Float::Float(int&& other)
  {
    this->_pointer = new float(std::move(other));
  }

  Float::Float(const int& other)
  {
    this->_pointer = new float(other);
  }

  Float& Float::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Float& Float::operator=(float&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new float(std::move(other));
    }
    return *this;
  }
	
  Float& Float::operator=(const float& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new float(other);
    }
    return *this;
  }

  Float& Float::operator=(Float&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Float& Float::operator=(const Float& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new float(*(float*)other._pointer);
    }
    return *this;
  }

  bool Float::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Float::operator==(float&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer == other);
  }
	
  bool Float::operator==(const float& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer == other);
  }
	
  bool Float::operator==(Float&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer == *(float*)other._pointer);
  }
	
  bool Float::operator==(const Float& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer == *(float*)other._pointer);
  }
	
  bool Float::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Float::operator!=(float&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer != other);
  }
	
  bool Float::operator!=(const float& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer != other);
  }
	
  bool Float::operator!=(Float&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer != *(float*)other._pointer);
  }

  bool Float::operator!=(const Float& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer != *(float*)other._pointer);
  }
	
  bool Float::operator<(const Float& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer < *(float*)other._pointer);
  }
	
  bool Float::operator<=(const Float& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer <= *(float*)other._pointer);
  }
	
  bool Float::operator>(const Float& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer > *(float*)other._pointer);
  }
	
  bool Float::operator>=(const Float& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(float*)this->_pointer >= *(float*)other._pointer);
  }

  Float Float::operator+(Float other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(float*)this->_pointer + *(float*)other._pointer;
  }
	
  Float& Float::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(float*)this->_pointer)++;
    return *this;
  }
	
  Float& Float::operator++(int)
  {
    this->Float::operator++();
    return *this;
  }
	
  Float Float::operator-(Float other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(float*)this->_pointer - *(float*)other._pointer;
  }
	
  Float& Float::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(float*)this->_pointer)--;
    return *this;
  }
	
  Float Float::operator*(Float other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(float*)this->_pointer * *(float*)other._pointer;
  }
	
  Float Float::operator/(Float other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(float*)this->_pointer / *(float*)other._pointer;
  }
	
  void Float::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (float*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const float& Float::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(float*)this->_pointer;
  }

  std::string Float::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(float*)this->_pointer);
  }

  Float::~Float()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Float&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_float(*(float*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Float& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_float(*(float*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Float& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new float();
      instream.read_std_float(*(float*)object._pointer);
    }
    return instream;
  }

}
