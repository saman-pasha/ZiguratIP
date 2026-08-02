
#ifndef __SHORT_HPP__
#define __SHORT_HPP__

#include "typeobject.hpp"

namespace Zigurat
{

  class Int;
  class String;

  class Short : public Object
  {
  public:
    typedef int16_t SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Short();
    Short(std::nullptr_t);
    Short(int16_t&&);
    Short(const int16_t&);
    Short(Short&&);
    Short(const Short&);
    Short(int&&);
    Short(const int&);
    Short(Int&&);
    Short(const Int&);

    Short& operator=(std::nullptr_t);
    Short& operator=(int16_t&&);
    Short& operator=(const int16_t&);
    Short& operator=(Short&&);
    Short& operator=(const Short&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(int16_t&&) const;
    virtual bool operator==(const int16_t&) const;
    virtual bool operator==(Short&&) const;
    virtual bool operator==(const Short&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(int16_t&&) const;
    virtual bool operator!=(const int16_t&) const;
    virtual bool operator!=(Short&&) const;
    virtual bool operator!=(const Short&) const;
    
    virtual bool operator<(const Short&) const;
    virtual bool operator<=(const Short&) const;
    virtual bool operator>(const Short&) const;
    virtual bool operator>=(const Short&) const;

    virtual Short operator+(Short);
    virtual Short& operator++();
    virtual Short& operator++(int);
    virtual Short operator-(Short);
    virtual Short& operator--();
    virtual Short operator*(Short);
    virtual Short operator/(Short);
    virtual Short operator%(Short);

    virtual void set_null();
    virtual const int16_t& value() const;
    virtual std::string to_std_string() const override;

    virtual ~Short();

    friend binarystream& operator<<(binarystream&, Short&&);
    friend binarystream& operator<<(binarystream&, const Short&);
    friend binarystream& operator>>(binarystream&, Short&);
  };
	
}

#endif // __SHORT_HPP__
