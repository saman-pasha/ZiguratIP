
#ifndef __UBYTE_HPP__
#define __UBYTE_HPP__


#include <cstdint>
#include "typeobject.hpp"

namespace Zigurat
{

  class Int;
  class String;

  class UByte : public Object
  {
  public:
    typedef uint8_t SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    UByte();
    UByte(std::nullptr_t);
    UByte(uint8_t&&);
    UByte(const uint8_t&);
    UByte(UByte&&);
    UByte(const UByte&);
    UByte(int&&);
    UByte(const int&);
    UByte(Int&&);
    UByte(const Int&);

    UByte& operator=(std::nullptr_t);
    UByte& operator=(uint8_t&&);
    UByte& operator=(const uint8_t&);
    UByte& operator=(UByte&&);
    UByte& operator=(const UByte&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(uint8_t&&) const;
    virtual bool operator==(const uint8_t&) const;
    virtual bool operator==(UByte&&) const;
    virtual bool operator==(const UByte&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(uint8_t&&) const;
    virtual bool operator!=(const uint8_t&) const;
    virtual bool operator!=(UByte&&) const;
    virtual bool operator!=(const UByte&) const;
    
    virtual bool operator<(const UByte&) const;
    virtual bool operator<=(const UByte&) const;
    virtual bool operator>(const UByte&) const;
    virtual bool operator>=(const UByte&) const;

    virtual UByte operator+(UByte);
    virtual UByte& operator++();
    virtual UByte& operator++(int);
    virtual UByte operator-(UByte);
    virtual UByte& operator--();
    virtual UByte operator*(UByte);
    virtual UByte operator/(UByte);
    virtual UByte operator%(UByte);

    virtual void set_null();
    virtual const uint8_t& value() const;
    virtual std::string to_std_string() const override;

    virtual ~UByte();

    friend binarystream& operator<<(binarystream&, UByte&&);
    friend binarystream& operator<<(binarystream&, const UByte&);
    friend binarystream& operator>>(binarystream&, UByte&);
  };
	
}

#endif // __UBYTE_HPP__
