
#ifndef __STRING_H__
#define __STRING_H__

#include "typeobject.h"
#include "typebool.h"
#include <string>

namespace Zigurat
{

  class Char;
  class Byte;
  class UByte;
  class Short;
  class UShort;
  class Int;
  class UInt;
  class Long;
  class ULong;
  class Float;
  class Double;
  class Real;
  class Timestamp;
  class Text;
  
  class String : public Object
  {
  public:
    typedef std::string SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    String();
    String(std::nullptr_t);
    String(std::string&&);
    String(const std::string&);
    String(String&&);
    String(const String&);
    String(const char*);
    String(const char*, size_t);

    String& operator=(std::nullptr_t);
    String& operator=(std::string&&);
    String& operator=(const std::string&);
    String& operator=(String&&);
    String& operator=(const String&);
    String& operator=(const char*);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(std::string&&) const;
    virtual bool operator==(const std::string&) const;
    virtual bool operator==(String&&) const;
    virtual bool operator==(const String&) const;
    virtual bool operator==(const char*) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(std::string&&) const;
    virtual bool operator!=(const std::string&) const;
    virtual bool operator!=(String&&) const;
    virtual bool operator!=(const String&) const;
    virtual bool operator!=(const char*) const;
    
    virtual bool operator<(const String&) const;
    virtual bool operator<=(const String&) const;
    virtual bool operator>(const String&) const;
    virtual bool operator>=(const String&) const;

    virtual String operator+(const char*) const;
    friend String operator+(const char*, String);
    virtual String operator+(String) const;

    virtual void set_null();
    virtual const std::string& value() const;

    virtual size_t std_size() const;
    virtual ULong size() const;
    virtual ULong SIZE() const;

    virtual size_t std_length() const;
    virtual ULong length() const;
    virtual ULong LENGTH() const;

    // SQL style pattern match: % stands for any run of characters, _ for
    // exactly one. Returns NULL if either side is NULL.
    virtual Bool LIKE(const String&) const;

    virtual Char GET(ULong) const;
    virtual void SET(ULong, const Char);

    virtual Bool to_bool(bool = true) const;
    virtual Char to_char() const;
    virtual Byte to_byte() const;
    virtual UByte to_ubyte() const;
    virtual Short to_short() const;
    virtual UShort to_ushort() const;
    virtual Int to_int() const;
    virtual UInt to_uint() const;
    virtual Long to_long() const;
    virtual ULong to_ulong() const;
    virtual Float to_float() const;
    virtual Double to_double() const;
    virtual Real to_real() const;
    virtual Timestamp to_timestamp(String = "%G/%m/%d %H:%M:%S", Bool = true) const;
    virtual Text to_text() const;

    virtual Bool TO_BOOL(Bool = true) const;
    virtual Char TO_CHAR() const;
    virtual Byte TO_BYTE() const;
    virtual UByte TO_UBYTE() const;
    virtual Short TO_SHORT() const;
    virtual UShort TO_USHORT() const;
    virtual Int TO_INT() const;
    virtual UInt TO_UINT() const;
    virtual Long TO_LONG() const;
    virtual ULong TO_ULONG() const;
    virtual Float TO_FLOAT() const;
    virtual Double TO_DOUBLE() const;
    virtual Real TO_REAL() const;
    virtual Timestamp TO_TIMESTAMP(String = "%G/%m/%d %H:%M:%S", Bool = true) const;
    virtual Text TO_TEXT() const;

    virtual int64_t pack_size() const override;
    virtual std::string to_std_string() const override;

    virtual ~String();

    friend binarystream& operator<<(binarystream&, String&&);
    friend binarystream& operator<<(binarystream&, const String&);
    friend binarystream& operator>>(binarystream&, String&);
  };
	
}

#endif // __STRING_H__
