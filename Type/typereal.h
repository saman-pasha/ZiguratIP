
#ifndef __REAL_H__
#define __REAL_H__

#include "typeobject.h"

namespace Zigurat
{

  class String;

  class Real : public Object
  {
  public:
    typedef long double SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Real();
    Real(std::nullptr_t);
    Real(long double&&);
    Real(const long double&);
    Real(Real&&);
    Real(const Real&);
    Real(int&&);
    Real(const int&);

    Real& operator=(std::nullptr_t);
    Real& operator=(long double&&);
    Real& operator=(const long double&);
    Real& operator=(Real&&);
    Real& operator=(const Real&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(long double&&) const;
    virtual bool operator==(const long double&) const;
    virtual bool operator==(Real&&) const;
    virtual bool operator==(const Real&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(long double&&) const;
    virtual bool operator!=(const long double&) const;
    virtual bool operator!=(Real&&) const;
    virtual bool operator!=(const Real&) const;
    
    virtual bool operator<(const Real&) const;
    virtual bool operator<=(const Real&) const;
    virtual bool operator>(const Real&) const;
    virtual bool operator>=(const Real&) const;

    virtual Real operator+(Real);
    virtual Real& operator++();
    virtual Real& operator++(int);
    virtual Real operator-(Real);
    virtual Real& operator--();
    virtual Real operator*(Real);
    virtual Real operator/(Real);

    virtual void set_null();
    virtual const long double& value() const;
    virtual std::string to_std_string() const override;

    virtual ~Real();

    friend binarystream& operator<<(binarystream&, Real&&);
    friend binarystream& operator<<(binarystream&, const Real&);
    friend binarystream& operator>>(binarystream&, Real&);
  };
	
}

#endif // __REAL_H__
