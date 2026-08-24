
#ifndef __LONG_HPP__
#define __LONG_HPP__


#include <cstdint>
#include <type_traits>
#include "typeobject.hpp"

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
    // The mirror of ULong's integral template: int64_t is `long' on
    // LP64 Linux and `long long' on macOS, so the other platform's
    // spelling has no exact constructor without this.
    template <typename T,
              typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    Long(T v) : Long(static_cast<int64_t>(v)) {}
    Long(Int&&);
    Long(const Int&);

    Long& operator=(std::nullptr_t);
    Long& operator=(int64_t&&);
    Long& operator=(const int64_t&);
    Long& operator=(Long&&);
    Long& operator=(const Long&);
    // The same integral template as the constructor, for the same
    // reason: `x = 0ul' on macOS converts equally well to int64_t's
    // rvalue and lvalue overloads and resolves to neither. The named
    // lvalue picks the const& overload without a second decision.
    template <typename T,
              typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    Long& operator=(T v)
    { const int64_t t = static_cast<int64_t>(v); return this->operator=(t); }

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

    // and once more for the two comparisons written against literals
    template <typename T,
              typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    bool operator==(T v) const
    { const int64_t t = static_cast<int64_t>(v); return this->operator==(t); }
    template <typename T,
              typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
    bool operator!=(T v) const
    { const int64_t t = static_cast<int64_t>(v); return this->operator!=(t); }
    
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

#endif // __LONG_HPP__
