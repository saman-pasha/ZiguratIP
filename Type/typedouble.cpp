#include "typedouble.hpp"
#include "typestring.hpp"
#include "binarystream.hpp"


namespace Zigurat
{

  const uint8_t Double::TDB = TDByte::DOUBLE;

  uint8_t Double::tdb() const
  {
    return (this->_pointer == nullptr) ? Double::TDB | TDByte::IS_NULL : Double::TDB;
  }

  Double::Double()
  {
    this->_pointer = new double();
  }
  
  Double::Double(std::nullptr_t)
  {

  }

  Double::Double(double&& other)
  {
    this->_pointer = new double(std::move(other));
  }

  Double::Double(const double& other)
  {
    this->_pointer = new double(other);
  }

  Double::Double(Double&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Double::Double(const Double& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new double(*(double*)other._pointer);
  }

  Double::Double(int&& other)
  {
    this->_pointer = new double(std::move(other));
  }

  Double::Double(const int& other)
  {
    this->_pointer = new double(other);
  }

  Double& Double::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Double& Double::operator=(double&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new double(std::move(other));
    }
    return *this;
  }
	
  Double& Double::operator=(const double& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new double(other);
    }
    return *this;
  }

  Double& Double::operator=(Double&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Double& Double::operator=(const Double& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new double(*(double*)other._pointer);
    }
    return *this;
  }

  bool Double::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Double::operator==(double&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer == other);
  }
	
  bool Double::operator==(const double& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer == other);
  }
	
  bool Double::operator==(Double&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer == *(double*)other._pointer);
  }
	
  bool Double::operator==(const Double& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer == *(double*)other._pointer);
  }
	
  bool Double::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Double::operator!=(double&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer != other);
  }
	
  bool Double::operator!=(const double& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer != other);
  }
	
  bool Double::operator!=(Double&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer != *(double*)other._pointer);
  }

  bool Double::operator!=(const Double& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer != *(double*)other._pointer);
  }
	
  bool Double::operator<(const Double& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer < *(double*)other._pointer);
  }
	
  bool Double::operator<=(const Double& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer <= *(double*)other._pointer);
  }
	
  bool Double::operator>(const Double& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer > *(double*)other._pointer);
  }
	
  bool Double::operator>=(const Double& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(double*)this->_pointer >= *(double*)other._pointer);
  }

  Double Double::operator+(Double other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(double*)this->_pointer + *(double*)other._pointer;
  }
	
  Double& Double::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(double*)this->_pointer)++;
    return *this;
  }
	
  Double& Double::operator++(int)
  {
    this->Double::operator++();
    return *this;
  }
	
  Double Double::operator-(Double other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(double*)this->_pointer - *(double*)other._pointer;
  }
	
  Double& Double::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(double*)this->_pointer)--;
    return *this;
  }
	
  Double Double::operator*(Double other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(double*)this->_pointer * *(double*)other._pointer;
  }
	
  Double Double::operator/(Double other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(double*)this->_pointer / *(double*)other._pointer;
  }
	
  void Double::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (double*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const double& Double::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(double*)this->_pointer;
  }

  std::string Double::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(double*)this->_pointer);
  }

  Double::~Double()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Double&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_double(*(double*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Double& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_double(*(double*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Double& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new double();
      instream.read_std_double(*(double*)object._pointer);
    }
    return instream;
  }

}
