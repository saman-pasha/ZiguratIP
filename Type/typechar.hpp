
#ifndef __CHAR_HPP__
#define __CHAR_HPP__


#include <cstdint>
#include "typeobject.hpp"

namespace Zigurat
{
  
  class String;

  class Char : public Object
  {
  public:
    typedef char SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Char();
    Char(std::nullptr_t);
    Char(char&&);
    Char(const char&);
    Char(Char&&);
    Char(const Char&);
    Char(const char*);

    Char& operator=(std::nullptr_t);
    Char& operator=(char&&);
    Char& operator=(const char&);
    Char& operator=(Char&&);
    Char& operator=(const Char&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(char&&) const;
    virtual bool operator==(const char&) const;
    virtual bool operator==(Char&&) const;
    virtual bool operator==(const Char&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(char&&) const;
    virtual bool operator!=(const char&) const;
    virtual bool operator!=(Char&&) const;
    virtual bool operator!=(const Char&) const;
    
    virtual bool operator<(const Char&) const;
    virtual bool operator<=(const Char&) const;
    virtual bool operator>(const Char&) const;
    virtual bool operator>=(const Char&) const;

    virtual void set_null();
    virtual const char& value() const;
    virtual std::string to_std_string() const override;

    virtual ~Char();

    friend binarystream& operator<<(binarystream&, Char&&);
    friend binarystream& operator<<(binarystream&, const Char&);
    friend binarystream& operator>>(binarystream&, Char&);
  };

}

#endif // __CHAR_HPP__
