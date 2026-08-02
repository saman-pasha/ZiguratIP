#include "typeobject.hpp"
#include "typebool.hpp"
#include "typestring.hpp"
#include "binarystream.hpp"
#include "textstream.hpp"


namespace Zigurat
{

  const uint8_t Object::TDB = TDByte::OBJECT;
  const std::string Object::NULL_STRING = "UNKNOWN";
  const ZiguratException Object::NULL_EXCEPTION(1000, "NULL value");

  uint8_t Object::tdb() const
  {
    return (this->_pointer == nullptr) ? Object::TDB | TDByte::IS_NULL : Object::TDB;
  }

  Bool Object::is_null() const
  {
    return this->_pointer == nullptr;
  }

  Bool Object::IS_NULL() const
  {
    return this->is_null();
  }

  const void* Object::pointer() const
  {
    return this->_pointer;
  }

  int64_t Object::pack_size() const
  {
    return (this->_pointer == nullptr) ? 1 : TDByte::SIZEOF(this->tdb()) + 1;
  }

  std::string Object::to_std_string() const
  {
    return "<Object: " + std::to_string((int64_t)this->_pointer) + ">";
  }

  String Object::to_string() const
  {
    return this->to_std_string();
  }

  String Object::TO_STRING() const
  {
    return this->to_std_string();
  }

  Object::~Object()
  {
    
  }

  binarystream& operator<<(binarystream& outstream, Object&& object)
  {
    outstream.write_std_ubyte(object.tdb());
    return outstream;
  }
  
  binarystream& operator<<(binarystream& outstream, const Object& object)
  {
    outstream.write_std_ubyte(object.tdb());
    return outstream;
  }
  
  binarystream& operator>>(binarystream& instream, Object& object)
  {
    instream.read_std_ubyte();
    return instream;
  }

  // to_std_string(), not to_string(): the latter returns a String, which is
  // itself an Object, so it selected this very overload again and recursed
  // until the stack ran out. Every ECHO of a value went that way.
  textstream& operator<<(textstream& outstream, Object&& object)
  {
    static_cast<std::ostream&>(outstream) << object.to_std_string();
    return outstream;
  }

  textstream& operator<<(textstream& outstream, const Object& object)
  {
    static_cast<std::ostream&>(outstream) << object.to_std_string();
    return outstream;
  }  

}
