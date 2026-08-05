
#ifndef __BYTE_HPP__
#define __BYTE_HPP__


#include <cstdint>
#include "typeobject.hpp"

namespace Zigurat
{

  class Int;
  class String;

  class Byte : public Object
  {
  public:
    typedef int8_t SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Byte();
    Byte(std::nullptr_t);
    Byte(int8_t&&);
    Byte(const int8_t&);
    Byte(Byte&&);
    Byte(const Byte&);
    Byte(int&&);
    Byte(const int&);
    Byte(Int&&);
    Byte(const Int&);

    Byte& operator=(std::nullptr_t);
    Byte& operator=(int8_t&&);
    Byte& operator=(const int8_t&);
    Byte& operator=(Byte&&);
    Byte& operator=(const Byte&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(int8_t&&) const;
    virtual bool operator==(const int8_t&) const;
    virtual bool operator==(Byte&&) const;
    virtual bool operator==(const Byte&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(int8_t&&) const;
    virtual bool operator!=(const int8_t&) const;
    virtual bool operator!=(Byte&&) const;
    virtual bool operator!=(const Byte&) const;
    
    virtual bool operator<(const Byte&) const;
    virtual bool operator<=(const Byte&) const;
    virtual bool operator>(const Byte&) const;
    virtual bool operator>=(const Byte&) const;

    virtual Byte operator+(Byte);
    virtual Byte& operator++();
    virtual Byte& operator++(int);
    virtual Byte operator-(Byte);
    virtual Byte& operator--();
    virtual Byte operator*(Byte);
    virtual Byte operator/(Byte);
    virtual Byte operator%(Byte);

    virtual void set_null();
    virtual const int8_t& value() const;
    virtual std::string to_std_string() const override;

    virtual ~Byte();

    friend binarystream& operator<<(binarystream&, Byte&&);
    friend binarystream& operator<<(binarystream&, const Byte&);
    friend binarystream& operator>>(binarystream&, Byte&);
  };
	
}

#endif // __BYTE_HPP__
