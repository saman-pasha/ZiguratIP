
#ifndef __LONG_H__
#define __LONG_H__

#include "typeobject.h"

namespace Zigurat
{

  class Int;
  class String;
	
  class Long : public Object
  {
  public:
    typedef int64_t SCT;
    static const uint8_t TDB;
    static const Long MIN;
    static const Long MAX;

    virtual uint8_t tdb() const override;

    Long();
    Long(std::nullptr_t);
    Long(int64_t&&);
    Long(const int64_t&);
    Long(Long&&);
    Long(const Long&);
    Long(int&&);
    Long(const int&);
    Long(Int&&);
    Long(const Int&);

    Long& operator=(std::nullptr_t);
    Long& operator=(int64_t&&);
    Long& operator=(const int64_t&);
    Long& operator=(Long&&);
    Long& operator=(const Long&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(int64_t&&) const;
    virtual bool operator==(const int64_t&) const;
    virtual bool operator==(Long&&) const;
    virtual bool operator==(const Long&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(int64_t&&) const;
    virtual bool operator!=(const int64_t&) const;
    virtual bool operator!=(Long&&) const;
    virtual bool operator!=(const Long&) const;
    
    virtual bool operator<(const Long&) const;
    virtual bool operator<=(const Long&) const;
    virtual bool operator>(const Long&) const;
    virtual bool operator>=(const Long&) const;

    virtual Long operator+(Long);
    virtual Long& operator++();
    virtual Long& operator++(int);
    virtual Long operator-(Long);
    virtual Long& operator--();
    virtual Long operator*(Long);
    virtual Long operator/(Long);
    virtual Long operator%(Long);

    virtual void set_null();
    virtual const int64_t& value() const;
    virtual std::string to_std_string() const override;

    virtual ~Long();

    friend binarystream& operator<<(binarystream&, Long&&);
    friend binarystream& operator<<(binarystream&, const Long&);
    friend binarystream& operator>>(binarystream&, Long&);
  };
	
}

#endif // __LONG_H__
