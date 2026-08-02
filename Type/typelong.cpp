#include "typelong.hpp"
#include "typeint.hpp"
#include "typestring.hpp"
#include <limits>
#include "binarystream.hpp"


namespace Zigurat
{

  const uint8_t Long::TDB = TDByte::LONG;

  const Long Long::MIN = std::numeric_limits<int64_t>::min();
  const Long Long::MAX = std::numeric_limits<int64_t>::max();
  
  uint8_t Long::tdb() const
  {
    return (this->_pointer == nullptr) ? Long::TDB | TDByte::IS_NULL : Long::TDB;
  }

  Long::Long()
  {
    this->_pointer = new int64_t();
  }
  
  Long::Long(std::nullptr_t)
  {

  }

  Long::Long(int64_t&& other)
  {
    this->_pointer = new int64_t(std::move(other));
  }

  Long::Long(const int64_t& other)
  {
    this->_pointer = new int64_t(other);
  }

  Long::Long(Long&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Long::Long(const Long& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new int64_t(*(int64_t*)other._pointer);
  }

  Long::Long(int&& other)
  {
    this->_pointer = new int64_t(std::move(other));
  }

  Long::Long(const int& other)
  {
    this->_pointer = new int64_t(other);
  }
	
  Long::Long(Int&& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new int64_t(other.value());
  }

  Long::Long(const Int& other)
  {
    if (other.pointer() != nullptr)
      this->_pointer = new int64_t(other.value());
  }

  Long& Long::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Long& Long::operator=(int64_t&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new int64_t(std::move(other));
    }
    return *this;
  }
	
  Long& Long::operator=(const int64_t& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new int64_t(other);
    }
    return *this;
  }

  Long& Long::operator=(Long&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Long& Long::operator=(const Long& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new int64_t(*(int64_t*)other._pointer);
    }
    return *this;
  }

  bool Long::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Long::operator==(int64_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer == other);
  }
	
  bool Long::operator==(const int64_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer == other);
  }
	
  bool Long::operator==(Long&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer == *(int64_t*)other._pointer);
  }
	
  bool Long::operator==(const Long& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer == *(int64_t*)other._pointer);
  }
	
  bool Long::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Long::operator!=(int64_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer != other);
  }
	
  bool Long::operator!=(const int64_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer != other);
  }
	
  bool Long::operator!=(Long&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer != *(int64_t*)other._pointer);
  }

  bool Long::operator!=(const Long& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer != *(int64_t*)other._pointer);
  }
	
  bool Long::operator<(const Long& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer < *(int64_t*)other._pointer);
  }
	
  bool Long::operator<=(const Long& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer <= *(int64_t*)other._pointer);
  }
	
  bool Long::operator>(const Long& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer > *(int64_t*)other._pointer);
  }
	
  bool Long::operator>=(const Long& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(int64_t*)this->_pointer >= *(int64_t*)other._pointer);
  }

  Long Long::operator+(Long other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int64_t*)this->_pointer + *(int64_t*)other._pointer;
  }
	
  Long& Long::operator++()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(int64_t*)this->_pointer)++;
    return *this;
  }
	
  Long& Long::operator++(int)
  {
    this->Long::operator++();
    return *this;
  }
	
  Long Long::operator-(Long other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int64_t*)this->_pointer - *(int64_t*)other._pointer;
  }
	
  Long& Long::operator--()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(int64_t*)this->_pointer)--;
    return *this;
  }
	
  Long Long::operator*(Long other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int64_t*)this->_pointer * *(int64_t*)other._pointer;
  }
	
  Long Long::operator/(Long other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int64_t*)this->_pointer / *(int64_t*)other._pointer;
  }
	
  Long Long::operator%(Long other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(int64_t*)this->_pointer % *(int64_t*)other._pointer;
  }
	
  void Long::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (int64_t*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const int64_t& Long::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(int64_t*)this->_pointer;
  }

  std::string Long::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    return std::to_string(*(int64_t*)this->_pointer);
  }

  Long::~Long()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Long&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_long(*(int64_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Long& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_long(*(int64_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Long& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new int64_t();
      instream.read_std_long(*(int64_t*)object._pointer);
    }
    return instream;
  }
	
}
