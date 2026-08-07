
#ifndef __TIMESTAMP_HPP__
#define __TIMESTAMP_HPP__


#include <cstdint>
#include "typeobject.hpp"
#include "typebool.hpp"
#include <ctime>

namespace Zigurat
{

  class String;

  class Timestamp : public Object
  {
  public:
    typedef time_t SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Timestamp();
    Timestamp(std::nullptr_t);
    Timestamp(time_t&&);
    Timestamp(const time_t&);
    Timestamp(Timestamp&&);
    Timestamp(const Timestamp&);
    Timestamp(int) = delete;

    Timestamp& operator=(std::nullptr_t);
    Timestamp& operator=(time_t&&);
    Timestamp& operator=(const time_t&);
    Timestamp& operator=(Timestamp&&);
    Timestamp& operator=(const Timestamp&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(time_t&&) const;
    virtual bool operator==(const time_t&) const;
    virtual bool operator==(Timestamp&&) const;
    virtual bool operator==(const Timestamp&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(time_t&&) const;
    virtual bool operator!=(const time_t&) const;
    virtual bool operator!=(Timestamp&&) const;
    virtual bool operator!=(const Timestamp&) const;
    
    virtual bool operator<(const Timestamp&) const;
    virtual bool operator<=(const Timestamp&) const;
    virtual bool operator>(const Timestamp&) const;
    virtual bool operator>=(const Timestamp&) const;

    virtual Timestamp operator+(Timestamp);
    virtual Timestamp operator-(Timestamp);

    virtual void set_null();
    virtual const time_t& value() const;

    static time_t std_now();
    static Timestamp now();
    static Timestamp NOW();

    virtual std::string to_std_string(std::string, bool) const;
    virtual std::string to_std_string(std::string) const;
    virtual std::string to_std_string(bool) const;
    virtual std::string to_std_string() const override;
    virtual String to_string() const override;
    virtual String TO_STRING() const override;
    virtual String to_string(String, Bool) const;
    virtual String TO_STRING(String, Bool) const;
    virtual String to_string(String) const;
    virtual String TO_STRING(String) const;
    virtual String to_string(Bool) const;
    virtual String TO_STRING(Bool) const;

    virtual ~Timestamp();

    friend binarystream& operator<<(binarystream&, Timestamp&&);
    friend binarystream& operator<<(binarystream&, const Timestamp&);
    friend binarystream& operator>>(binarystream&, Timestamp&);
  };
	
}

#endif // __TIMESTAMP_HPP__
