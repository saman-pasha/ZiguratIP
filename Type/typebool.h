
#ifndef __BOOL_H__
#define __BOOL_H__

#include "typeobject.h"

namespace Zigurat
{

  class String;

  class Bool : public Object
  {
  public:
    typedef bool SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Bool();
    Bool(std::nullptr_t);
    Bool(bool&&);
    Bool(const bool&);
    Bool(Bool&&);
    Bool(const Bool&);

    Bool& operator=(std::nullptr_t);
    Bool& operator=(bool&&);
    Bool& operator=(const bool&);
    Bool& operator=(Bool&&);
    Bool& operator=(const Bool&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(bool&&) const;
    virtual bool operator==(const bool&) const;
    virtual bool operator==(Bool&&) const;
    virtual bool operator==(const Bool&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(bool&&) const;
    virtual bool operator!=(const bool&) const;
    virtual bool operator!=(Bool&&) const;
    virtual bool operator!=(const Bool&) const;

    virtual operator bool() const;

    virtual void set_null();
    virtual const bool& value() const;
    virtual std::string to_std_string(bool) const;
    virtual std::string to_std_string() const override;
    virtual String to_string() const override;
    virtual String TO_STRING() const override;
    virtual String to_string(Bool) const;
    virtual String TO_STRING(Bool) const;

    virtual ~Bool();

    friend binarystream& operator<<(binarystream&, Bool&&);
    friend binarystream& operator<<(binarystream&, const Bool&);
    friend binarystream& operator>>(binarystream&, Bool&);
  };
  
}

#endif // __BOOL_H__
