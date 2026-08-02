#include "typeshort.hpp"
#include "typeint.hpp"
#include "typestring.hpp"
#include "binarystream.hpp"


namespace Zigurat
{

  const uint8_t Short::TDB = TDByte::SHORT;

  uint8_t Short::tdb() const
  {
    return (this->_pointer == nullptr) ? Short::TDB | TDByte::IS_NULL : Short::TDB;
  }

  Short::Short()
  {
    this->_pointer = new int16_t();
  }
  
  Short::Short(std::nullptr_t)
  {

  }

  Short::Short(int16_t&& other)
  {
    this->_pointer = new int16_t(std::move(other));
  }

  Short::Short(const int16_t& other)
  {
    this->_pointer = new int16_t(other);
  }

  Short::Short(Short&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Short::Short(const Short& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new int16_t(*(int16_t*)other._pointer);
  }

  Short::Short(int&& other)
  {
    this->_pointer = new int16_t(std::move(other));
  }

  Short::Short(const int& other)
  {
    this->_pointer = new int16_t(other);
  }
	
  Short::Short(Int&& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new int16_t(other.value());
  }
		
  Short::Short(const Int& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new int16_t(other.value());
  }
	
  Short& Short::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Short& Short::operator=(int16_t&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new int16_t(std::move(other));
    }
    return *this;
  }
	
  Short& Short::operator=(const int16_t& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new int16_t(other);
    }
    return *this;
  }

  Short& Short::operator=(Short&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Short& Short::operator=(const Short& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new int16_t(*(int16_t*)other._pointer);
    }
    return *this;
  }

  bool Short::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Short::operator==(int16_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer == other);
  }
	
  bool Short::operator==(const int16_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer == other);
  }
	
  bool Short::operator==(Short&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer == *(int16_t*)other._pointer);
  }
	
  bool Short::operator==(const Short& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer == *(int16_t*)other._pointer);
  }
	
  bool Short::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Short::operator!=(int16_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer != other);
  }
	
  bool Short::operator!=(const int16_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer != other);
  }
	
  bool Short::operator!=(Short&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer != *(int16_t*)other._pointer);
  }

  bool Short::operator!=(const Short& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer != *(int16_t*)other._pointer);
  }
	
  bool Short::operator<(const Short& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer < *(int16_t*)other._pointer);
  }
	
  bool Short::operator<=(const Short& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer <= *(int16_t*)other._pointer);
  }
	
  bool Short::operator>(const Short& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer > *(int16_t*)other._pointer);
  }
	
  bool Short::operator>=(const Short& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int16_t*)this->_pointer >= *(int16_t*)other._pointer);
  }

  Short Short::operator+(Short other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int16_t*)this->_pointer + *(int16_t*)other._pointer;
  }
	
  Short& Short::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(int16_t*)this->_pointer)++;
    return *this;
  }
	
  Short& Short::operator++(int)
  {
    this->Short::operator++();
    return *this;
  }
	
  Short Short::operator-(Short other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int16_t*)this->_pointer - *(int16_t*)other._pointer;
  }
	
  Short& Short::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(int16_t*)this->_pointer)--;
    return *this;
  }
	
  Short Short::operator*(Short other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int16_t*)this->_pointer * *(int16_t*)other._pointer;
  }
	
  Short Short::operator/(Short other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int16_t*)this->_pointer / *(int16_t*)other._pointer;
  }
	
  Short Short::operator%(Short other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int16_t*)this->_pointer % *(int16_t*)other._pointer;
  }

  void Short::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (int16_t*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const int16_t& Short::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(int16_t*)this->_pointer;
  }

  std::string Short::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(int16_t*)this->_pointer);
  }

  Short::~Short()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Short&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_short(*(int16_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Short& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_short(*(int16_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Short& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new int16_t();
      instream.read_std_short(*(int16_t*)object._pointer);
    }
    return instream;
  }

}
