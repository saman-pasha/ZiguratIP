
#ifndef __USHORT_H__
#define __USHORT_H__

#include "typeobject.h"

namespace Zigurat
{

  class Int;
  class String;

  class UShort : public Object
  {
  public:
    typedef uint16_t SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    UShort();
    UShort(std::nullptr_t);
    UShort(uint16_t&&);
    UShort(const uint16_t&);
    UShort(UShort&&);
    UShort(const UShort&);
    UShort(int&&);
    UShort(const int&);
    UShort(Int&&);
    UShort(const Int&);

    UShort& operator=(std::nullptr_t);
    UShort& operator=(uint16_t&&);
    UShort& operator=(const uint16_t&);
    UShort& operator=(UShort&&);
    UShort& operator=(const UShort&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(uint16_t&&) const;
    virtual bool operator==(const uint16_t&) const;
    virtual bool operator==(UShort&&) const;
    virtual bool operator==(const UShort&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(uint16_t&&) const;
    virtual bool operator!=(const uint16_t&) const;
    virtual bool operator!=(UShort&&) const;
    virtual bool operator!=(const UShort&) const;
    
    virtual bool operator<(const UShort&) const;
    virtual bool operator<=(const UShort&) const;
    virtual bool operator>(const UShort&) const;
    virtual bool operator>=(const UShort&) const;

    virtual UShort operator+(UShort);
    virtual UShort& operator++();
    virtual UShort& operator++(int);
    virtual UShort operator-(UShort);
    virtual UShort& operator--();
    virtual UShort operator*(UShort);
    virtual UShort operator/(UShort);
    virtual UShort operator%(UShort);

    virtual void set_null();
    virtual const uint16_t& value() const;
    virtual std::string to_std_string() const override;

    virtual ~UShort();

    friend binarystream& operator<<(binarystream&, UShort&&);
    friend binarystream& operator<<(binarystream&, const UShort&);
    friend binarystream& operator>>(binarystream&, UShort&);
  };
	
}

#endif // __USHORT_H__
