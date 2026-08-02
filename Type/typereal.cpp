#include "typereal.hpp"
#include "typestring.hpp"
#include "binarystream.hpp"


namespace Zigurat
{

  const uint8_t Real::TDB = TDByte::REAL;

  uint8_t Real::tdb() const
  {
    return (this->_pointer == nullptr) ? Real::TDB | TDByte::IS_NULL : Real::TDB;
  }

  Real::Real()
  {
    this->_pointer = new long double();
  }
  
  Real::Real(std::nullptr_t)
  {

  }

  Real::Real(long double&& other)
  {
    this->_pointer = new long double(std::move(other));
  }

  Real::Real(const long double& other)
  {
    this->_pointer = new long double(other);
  }

  Real::Real(Real&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Real::Real(const Real& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new long double(*(long double*)other._pointer);
  }

  Real::Real(int&& other)
  {
    this->_pointer = new long double(std::move(other));
  }

  Real::Real(const int& other)
  {
    this->_pointer = new long double(other);
  }

  Real& Real::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Real& Real::operator=(long double&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new long double(std::move(other));
    }
    return *this;
  }
	
  Real& Real::operator=(const long double& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new long double(other);
    }
    return *this;
  }

  Real& Real::operator=(Real&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Real& Real::operator=(const Real& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new long double(*(long double*)other._pointer);
    }
    return *this;
  }

  bool Real::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Real::operator==(long double&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer == other);
  }
	
  bool Real::operator==(const long double& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer == other);
  }
	
  bool Real::operator==(Real&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer == *(long double*)other._pointer);
  }
	
  bool Real::operator==(const Real& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer == *(long double*)other._pointer);
  }
	
  bool Real::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Real::operator!=(long double&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer != other);
  }
	
  bool Real::operator!=(const long double& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer != other);
  }
	
  bool Real::operator!=(Real&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer != *(long double*)other._pointer);
  }

  bool Real::operator!=(const Real& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer != *(long double*)other._pointer);
  }
	
  bool Real::operator<(const Real& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer < *(long double*)other._pointer);
  }
	
  bool Real::operator<=(const Real& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer <= *(long double*)other._pointer);
  }
	
  bool Real::operator>(const Real& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer > *(long double*)other._pointer);
  }
	
  bool Real::operator>=(const Real& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(long double*)this->_pointer >= *(long double*)other._pointer);
  }

  Real Real::operator+(Real other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(long double*)this->_pointer + *(long double*)other._pointer;
  }
	
  Real& Real::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(long double*)this->_pointer)++;
    return *this;
  }
	
  Real& Real::operator++(int)
  {
    this->Real::operator++();
    return *this;
  }
	
  Real Real::operator-(Real other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(long double*)this->_pointer - *(long double*)other._pointer;
  }
	
  Real& Real::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(long double*)this->_pointer)--;
    return *this;
  }
	
  Real Real::operator*(Real other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(long double*)this->_pointer * *(long double*)other._pointer;
  }
	
  Real Real::operator/(Real other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(long double*)this->_pointer / *(long double*)other._pointer;
  }
	
  void Real::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (long double*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const long double& Real::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(long double*)this->_pointer;
  }

  std::string Real::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(long double*)this->_pointer);
  }

  Real::~Real()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Real&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_real(*(long double*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Real& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_real(*(long double*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Real& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new long double();
      instream.read_std_real(*(long double*)object._pointer);
    }
    return instream;
  }

}
