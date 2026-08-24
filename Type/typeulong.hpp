
#ifndef __ULONG_HPP__
#define __ULONG_HPP__


#include <cstdint>
#include <type_traits>
#include "typeobject.hpp"

namespace Zigurat
{

  class Int;
  class UInt;
  class String;

  class ULong : public Object
  {
  public:
    typedef uint64_t SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    ULong();
    ULong(std::nullptr_t);
    ULong(uint64_t&&);
    ULong(const uint64_t&);
    ULong(ULong&&);
    ULong(const ULong&);
    ULong(int&&);
    ULong(const int&);
    ULong(unsigned int&&);
    ULong(const unsigned int&);
    // Every remaining plain integral, exactly. uint64_t is `unsigned
    // long' on LP64 Linux and `unsigned long long' on macOS, so a
    // size_t, a 0ul literal or a strlen() matches no fixed-type
    // constructor on one platform or the other and drowns ambiguously
    // among equal-rank conversions instead. A constrained template is
    // an exact match wherever it is needed, and loses to the
    // non-template constructors everywhere it is not.
    template <typename T,
              typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    ULong(T v) : ULong(static_cast<uint64_t>(v)) {}
    ULong(Int&&);
    ULong(const Int&);
    ULong(UInt&&);
    ULong(const UInt&);

    ULong& operator=(std::nullptr_t);
    ULong& operator=(uint64_t&&);
    ULong& operator=(const uint64_t&);
    ULong& operator=(ULong&&);
    ULong& operator=(const ULong&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(uint64_t&&) const;
    virtual bool operator==(const uint64_t&) const;
    virtual bool operator==(ULong&&) const;
    virtual bool operator==(const ULong&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(uint64_t&&) const;
    virtual bool operator!=(const uint64_t&) const;
    virtual bool operator!=(ULong&&) const;
    virtual bool operator!=(const ULong&) const;
    
    virtual bool operator<(const ULong&) const;
    virtual bool operator<=(const ULong&) const;
    virtual bool operator>(const ULong&) const;
    virtual bool operator>=(const ULong&) const;

    virtual ULong operator+(ULong);
    virtual ULong& operator++();
    virtual ULong& operator++(int);
    virtual ULong operator-(ULong);
    virtual ULong& operator--();
    virtual ULong operator*(ULong);
    virtual ULong operator/(ULong);
    virtual ULong operator%(ULong);

    virtual void set_null();
    virtual const uint64_t& value() const;
    virtual std::string to_std_string() const override;

    virtual ~ULong();

    friend binarystream& operator<<(binarystream&, ULong&&);
    friend binarystream& operator<<(binarystream&, const ULong&);
    friend binarystream& operator>>(binarystream&, ULong&);
  };
	
}

#endif // __ULONG_HPP__
