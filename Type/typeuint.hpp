
#ifndef __UINT_HPP__
#define __UINT_HPP__


#include <cstdint>
#include "typeobject.hpp"

namespace Zigurat
{

  class Int;
  class String;

  class UInt : public Object
  {
  public:
    typedef uint32_t SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    UInt();
    UInt(std::nullptr_t);
    UInt(uint32_t&&);
    UInt(const uint32_t&);
    UInt(UInt&&);
    UInt(const UInt&);
    UInt(int&&);
    UInt(const int&);
    UInt(Int&&);
    UInt(const Int&);

    UInt& operator=(std::nullptr_t);
    UInt& operator=(uint32_t&&);
    UInt& operator=(const uint32_t&);
    UInt& operator=(UInt&&);
    UInt& operator=(const UInt&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(uint32_t&&) const;
    virtual bool operator==(const uint32_t&) const;
    virtual bool operator==(UInt&&) const;
    virtual bool operator==(const UInt&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(uint32_t&&) const;
    virtual bool operator!=(const uint32_t&) const;
    virtual bool operator!=(UInt&&) const;
    virtual bool operator!=(const UInt&) const;
    
    virtual bool operator<(const UInt&) const;
    virtual bool operator<=(const UInt&) const;
    virtual bool operator>(const UInt&) const;
    virtual bool operator>=(const UInt&) const;

    virtual UInt operator+(UInt);
    virtual UInt& operator++();
    virtual UInt& operator++(int);
    virtual UInt operator-(UInt);
    virtual UInt& operator--();
    virtual UInt operator*(UInt);
    virtual UInt operator/(UInt);
    virtual UInt operator%(UInt);

    virtual void set_null();
    virtual const uint32_t& value() const;
    virtual std::string to_std_string() const override;

    virtual ~UInt();

    friend binarystream& operator<<(binarystream&, UInt&&);
    friend binarystream& operator<<(binarystream&, const UInt&);
    friend binarystream& operator>>(binarystream&, UInt&);
  };
	
}

#endif // __UINT_HPP__
