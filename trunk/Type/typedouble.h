
#ifndef __DOUBLE_H__
#define __DOUBLE_H__

#include "typeobject.h"

namespace Zigurat
{

  class String;

  class Double : public Object
  {
  public:
    typedef double SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Double();
    Double(std::nullptr_t);
    Double(double&&);
    Double(const double&);
    Double(Double&&);
    Double(const Double&);
    Double(int&&);
    Double(const int&);

    Double& operator=(std::nullptr_t);
    Double& operator=(double&&);
    Double& operator=(const double&);
    Double& operator=(Double&&);
    Double& operator=(const Double&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(double&&) const;
    virtual bool operator==(const double&) const;
    virtual bool operator==(Double&&) const;
    virtual bool operator==(const Double&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(double&&) const;
    virtual bool operator!=(const double&) const;
    virtual bool operator!=(Double&&) const;
    virtual bool operator!=(const Double&) const;
    
    virtual bool operator<(const Double&) const;
    virtual bool operator<=(const Double&) const;
    virtual bool operator>(const Double&) const;
    virtual bool operator>=(const Double&) const;

    virtual Double operator+(Double);
    virtual Double& operator++();
    virtual Double& operator++(int);
    virtual Double operator-(Double);
    virtual Double& operator--();
    virtual Double operator*(Double);
    virtual Double operator/(Double);

    virtual void set_null();
    virtual const double& value() const;
    virtual std::string to_std_string() const override;

    virtual ~Double();

    friend binarystream& operator<<(binarystream&, Double&&);
    friend binarystream& operator<<(binarystream&, const Double&);
    friend binarystream& operator>>(binarystream&, Double&);
  };
	
}

#endif // __DOUBLE_H__
