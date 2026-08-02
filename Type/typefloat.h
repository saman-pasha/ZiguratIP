
#ifndef __FLOAT_H__
#define __FLOAT_H__

#include "typeobject.h"

namespace Zigurat
{

  class String;

  class Float : public Object
  {
  public:
    typedef float SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Float();
    Float(std::nullptr_t);
    Float(float&&);
    Float(const float&);
    Float(Float&&);
    Float(const Float&);
    Float(int&&);
    Float(const int&);

    Float& operator=(std::nullptr_t);
    Float& operator=(float&&);
    Float& operator=(const float&);
    Float& operator=(Float&&);
    Float& operator=(const Float&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(float&&) const;
    virtual bool operator==(const float&) const;
    virtual bool operator==(Float&&) const;
    virtual bool operator==(const Float&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(float&&) const;
    virtual bool operator!=(const float&) const;
    virtual bool operator!=(Float&&) const;
    virtual bool operator!=(const Float&) const;
    
    virtual bool operator<(const Float&) const;
    virtual bool operator<=(const Float&) const;
    virtual bool operator>(const Float&) const;
    virtual bool operator>=(const Float&) const;

    virtual Float operator+(Float);
    virtual Float& operator++();
    virtual Float& operator++(int);
    virtual Float operator-(Float);
    virtual Float& operator--();
    virtual Float operator*(Float);
    virtual Float operator/(Float);

    virtual void set_null();
    virtual const float& value() const;
    virtual std::string to_std_string() const override;

    virtual ~Float();

    friend binarystream& operator<<(binarystream&, Float&&);
    friend binarystream& operator<<(binarystream&, const Float&);
    friend binarystream& operator>>(binarystream&, Float&);
  };
	
}

#endif // __FLOAT_H__
