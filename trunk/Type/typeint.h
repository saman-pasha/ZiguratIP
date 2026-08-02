
#ifndef __INT_H__
#define __INT_H__

#include "typeobject.h"

namespace Zigurat
{

  class String;

  class Int : public Object
  {
  public:
    typedef int32_t SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Int();
    Int(std::nullptr_t);
    Int(int32_t&&);
    Int(const int32_t&);
    Int(Int&&);
    Int(const Int&);
    
    Int& operator=(std::nullptr_t);
    Int& operator=(int32_t&&);
    Int& operator=(const int32_t&);
    Int& operator=(Int&&);
    Int& operator=(const Int&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(int32_t&&) const;
    virtual bool operator==(const int32_t&) const;
    virtual bool operator==(Int&&) const;
    virtual bool operator==(const Int&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(int32_t&&) const;
    virtual bool operator!=(const int32_t&) const;
    virtual bool operator!=(Int&&) const;
    virtual bool operator!=(const Int&) const;
    
    virtual bool operator<(const Int&) const;
    virtual bool operator<=(const Int&) const;
    virtual bool operator>(const Int&) const;
    virtual bool operator>=(const Int&) const;

    virtual Int operator+(Int);
    virtual Int& operator++();
    virtual Int& operator++(int);
    virtual Int operator-(Int);
    virtual Int& operator--();
    virtual Int operator*(Int);
    virtual Int operator/(Int);
    virtual Int operator%(Int);

    virtual void set_null();
    virtual const int32_t& value() const;
    virtual std::string to_std_string() const override;

    virtual ~Int();

    friend binarystream& operator<<(binarystream&, Int&&);
    friend binarystream& operator<<(binarystream&, const Int&);
    friend binarystream& operator>>(binarystream&, Int&);
  };
	
}

#endif // __INT_H__
