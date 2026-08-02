#include "typetimestamp.h"
#include "typestring.h"
#include <cstring>
#include "binarystream.h"


namespace Zigurat
{

  const uint8_t Timestamp::TDB = TDByte::TIMESTAMP;

  uint8_t Timestamp::tdb() const
  {
    return (this->_pointer == nullptr) ? Timestamp::TDB | TDByte::IS_NULL : Timestamp::TDB;
  }

  Timestamp::Timestamp()
  {
    this->_pointer = new time_t();
  }
  
  Timestamp::Timestamp(std::nullptr_t)
  {

  }

  Timestamp::Timestamp(time_t&& other)
  {
    this->_pointer = new time_t(std::move(other));
  }

  Timestamp::Timestamp(const time_t& other)
  {
    this->_pointer = new time_t(other);
  }

  Timestamp::Timestamp(Timestamp&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  Timestamp::Timestamp(const Timestamp& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new time_t(*(time_t*)other._pointer);
  }

  Timestamp& Timestamp::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  Timestamp& Timestamp::operator=(time_t&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new time_t(std::move(other));
    }
    return *this;
  }
	
  Timestamp& Timestamp::operator=(const time_t& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new time_t(other);
    }
    return *this;
  }

  Timestamp& Timestamp::operator=(Timestamp&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  Timestamp& Timestamp::operator=(const Timestamp& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new time_t(*(time_t*)other._pointer);
    }
    return *this;
  }

  bool Timestamp::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Timestamp::operator==(time_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer == other);
  }
	
  bool Timestamp::operator==(const time_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer == other);
  }
	
  bool Timestamp::operator==(Timestamp&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer == *(time_t*)other._pointer);
  }
	
  bool Timestamp::operator==(const Timestamp& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer == *(time_t*)other._pointer);
  }
	
  bool Timestamp::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  bool Timestamp::operator!=(time_t&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer != other);
  }
	
  bool Timestamp::operator!=(const time_t& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer != other);
  }
	
  bool Timestamp::operator!=(Timestamp&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer != *(time_t*)other._pointer);
  }

  bool Timestamp::operator!=(const Timestamp& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer != *(time_t*)other._pointer);
  }
	
  bool Timestamp::operator<(const Timestamp& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer < *(time_t*)other._pointer);
  }
	
  bool Timestamp::operator<=(const Timestamp& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer <= *(time_t*)other._pointer);
  }
	
  bool Timestamp::operator>(const Timestamp& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer > *(time_t*)other._pointer);
  }
	
  bool Timestamp::operator>=(const Timestamp& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(time_t*)this->_pointer >= *(time_t*)other._pointer);
  }

  Timestamp Timestamp::operator+(Timestamp other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(time_t*)this->_pointer + *(time_t*)other._pointer;
  }
	
  Timestamp Timestamp::operator-(Timestamp other)
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return *(time_t*)this->_pointer - *(time_t*)other._pointer;
  }
	
  void Timestamp::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (time_t*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  const time_t& Timestamp::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(time_t*)this->_pointer;
  }

  time_t Timestamp::std_now()
  {
    return std::time(0);
  }
	
  Timestamp Timestamp::now()
  {
    return Timestamp::std_now();
  }
	
  Timestamp Timestamp::NOW()
  {
    return Timestamp::std_now();
  }
	
  std::string Timestamp::to_std_string(std::string format, bool utc) const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    char buffer[256];
    struct tm* timeinfo = (utc) ? gmtime((time_t*)this->_pointer) : localtime((time_t*)this->_pointer);
    size_t length = strftime(buffer, 255, format.c_str(), timeinfo);
    delete timeinfo;
    return std::string(buffer, length - 1);
  }

  std::string Timestamp::to_std_string(std::string format) const
  {
    return this->to_std_string(format, false);
  }

  std::string Timestamp::to_std_string(bool utc) const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    char* date = asctime((utc) ? gmtime((time_t*)this->_pointer) : localtime((time_t*)this->_pointer));
    return std::string(date, strlen(date) - 1);
  }

  std::string Timestamp::to_std_string() const
  {
    return this->to_std_string(false);
  }

  String Timestamp::to_string() const
  {
    return this->to_std_string(false);
  }

  String Timestamp::TO_STRING() const
  {
    return this->to_std_string(false);
  }

  String Timestamp::to_string(String format, Bool utc) const
  {
    return this->to_std_string(format.value(), utc.value());
  }

  String Timestamp::TO_STRING(String format, Bool utc) const
  {
    return this->to_std_string(format.value(), utc.value());
  }

  String Timestamp::to_string(String format) const
  {
    return this->to_std_string(format.value(), false);
  }

  String Timestamp::TO_STRING(String format) const
  {
    return this->to_std_string(format.value(), false);
  }

  String Timestamp::to_string(Bool utc) const
  {
    return this->to_std_string(utc.value());
  }

  String Timestamp::TO_STRING(Bool utc) const
  {
    return this->to_std_string(utc.value());
  }

  Timestamp::~Timestamp()
  {
    this->set_null();
  }
	
  binarystream& operator<<(binarystream& outstream, Timestamp&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_time(*(time_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Timestamp& object)
  {
    outstream.write_std_ubyte(object.tdb());
    if (object._pointer != nullptr)
      outstream.write_std_time(*(time_t*)object._pointer);
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Timestamp& object)
  {
    uint8_t tdb = instream.read_std_ubyte();
    if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
      object.set_null();
    } else {
      if (object._pointer == nullptr) object._pointer = new time_t();
      instream.read_std_time(*(time_t*)object._pointer);
    }
    return instream;
  }

}
