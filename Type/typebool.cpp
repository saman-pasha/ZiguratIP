#include "typebool.h"
#include "typestring.h"
#include <sstream>
#include "binarystream.h"


namespace Zigurat
{

  const uint8_t Bool::TDB = TDByte::BOOL;

  uint8_t Bool::tdb() const
  {
    return (this->_pointer == nullptr) ? Bool::TDB | TDByte::IS_NULL : Bool::TDB;
  }

  Bool::Bool()
  {
    this->_pointer = new bool();
  }
  
  Bool::Bool(std::nullptr_t)
  {

  }

  Bool::Bool(bool&& other)
  {
    this->_pointer = new bool(std::move(other));
  }

  Bool::Bool(const bool& other)
  {
    this->_pointer = new bool(other);
  }

  Bool::Bool(Bool&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Bool::Bool(const Bool& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new bool(*(bool*)other._pointer);
  }

  Bool& Bool::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Bool& Bool::operator=(bool&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new bool(std::move(other));
    }
    return *this;
  }
	
  Bool& Bool::operator=(const bool& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new bool(other);
    }
    return *this;
  }

  Bool& Bool::operator=(Bool&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Bool& Bool::operator=(const Bool& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new bool(*(bool*)other._pointer);
    }
    return *this;
  }

  bool Bool::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Bool::operator==(bool&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(bool*)this->_pointer == other);
  }
	
  bool Bool::operator==(const bool& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(bool*)this->_pointer == other);
  }
	
  bool Bool::operator==(Bool&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(bool*)this->_pointer == *(bool*)other._pointer);
  }
	
  bool Bool::operator==(const Bool& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(bool*)this->_pointer == *(bool*)other._pointer);
  }
	
  bool Bool::operator!=(std::nullptr_t) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (this->_pointer != nullptr);
  }
	
  bool Bool::operator!=(bool&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(bool*)this->_pointer != other);
  }
	
  bool Bool::operator!=(const bool& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(bool*)this->_pointer != other);
  }
	
  bool Bool::operator!=(Bool&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(bool*)this->_pointer != *(bool*)other._pointer);
  }

  bool Bool::operator!=(const Bool& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(bool*)this->_pointer != *(bool*)other._pointer);
  }
	
  Bool::operator bool() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(bool*)this->_pointer;
  }

  void Bool::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (bool*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const bool& Bool::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(bool*)this->_pointer;
  }

  std::string Bool::to_std_string(bool alpha) const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    std::stringstream ss;
    if (alpha)
      ss << std::boolalpha << *(bool*)this->_pointer;
    else
      ss << *(bool*)this->_pointer;
    return ss.str();
  }

  std::string Bool::to_std_string() const
  {
    return this->to_std_string(true);
  }

  String Bool::to_string() const
  {
    return this->to_std_string(true);
  }

  String Bool::TO_STRING() const
  {
    return this->to_std_string(true);
  }

  String Bool::to_string(Bool alpha) const
  {
    return this->to_std_string(alpha.value());
  }

  String Bool::TO_STRING(Bool alpha) const
  {
    return this->to_std_string(alpha.value());
  }

  Bool::~Bool()
  {
    this->set_null();
  }

  binarystream& operator<<(binarystream& outstream, Bool&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_bool(*(bool*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Bool& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_bool(*(bool*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Bool& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new bool();
      instream.read_std_bool(*(bool*)object._pointer);
    }
    return instream;
  }

}
