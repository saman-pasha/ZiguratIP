
#ifndef __BIGINT_HPP__
#define __BIGINT_HPP__


#include <cstdint>
#include "polynomial.hpp"
#include "binarystream.hpp"
#include <iostream>

namespace Zigurat
{

  class BigInt
  {
  public:
    typedef uint32_t   word_t;
    typedef Polynomial array_t;

    static const word_t  POS_VAL;
    static const word_t  NEG_VAL;

  protected:
    array_t _a;
    bool    _sign;

  public:
    BigInt();
    BigInt(int32_t);
    BigInt(uint32_t);
    BigInt(int64_t);
    BigInt(uint64_t);
    BigInt(const uint8_t*, size_t, bool);
    BigInt(std::initializer_list<word_t>);
    BigInt(array_t&&, bool = false);
    BigInt(const array_t&, bool = false);
    BigInt(BigInt&&);
    BigInt(const BigInt&);

    BigInt& operator=(BigInt&&);
    BigInt& operator=(const BigInt&);

    bool   sign()    const;
    void   sign(bool);
    size_t size(bool)    const;
    size_t length(bool)  const;
    bool   is_zero() const;
    bool   is_odd()  const;
    bool   is_even() const;
    void   to_octet_string(uint8_t*, bool) const;
    void   to_octet_string(binarystream&, bool)  const;

    static int    cmp(const BigInt&, const BigInt&);
    static BigInt div(const BigInt&, const BigInt&, BigInt*);

    bool operator==(const BigInt&) const;
    bool operator!=(const BigInt&) const;
    bool operator< (const BigInt&) const;
    bool operator> (const BigInt&) const;
    bool operator<=(const BigInt&) const;
    bool operator>=(const BigInt&) const;

    BigInt operator<<(const size_t) const;
    BigInt operator>>(const size_t) const;

    BigInt operator+(const BigInt&) const;
    BigInt operator-(const BigInt&) const;
    BigInt operator*(const BigInt&) const;
    BigInt operator/(const BigInt&) const;
    BigInt operator%(const BigInt&) const;

    static BigInt pow(const BigInt&, const BigInt&);                    // base ^ exponent
    static BigInt mod_pow(const BigInt&, const BigInt&, const BigInt&); // (base ^ exponent) mod divisor
    static BigInt rand(size_t);                                         // Random Polynomial Generation of the size
    static BigInt rand(const BigInt, const BigInt);                     // Random Polynomial Generation between r and s
    static BigInt gcd(const BigInt&, const BigInt&);                    // Greatest Common Divisor
    static void   gcd(const BigInt&, const BigInt&, BigInt&, BigInt&);  // g is odd and gcd(a, b) = g × (2**d)
    static BigInt lcm(const BigInt&, const BigInt&);                    // Least Common Multiple
    static BigInt eed(const BigInt&, const BigInt&, BigInt&, BigInt&);  // Extended Euclidean Divisor a.c + b.d = gcd(a, b)
    static BigInt inverse(const BigInt&, const BigInt&);                // return t from a.t = 1 mod n

    void print(std::ostream&);
    std::string to_string();
    friend std::ostream& operator<<(std::ostream& os, BigInt num)
    {
      num.print(os);
      return os;
    }

    virtual ~BigInt();
  };

}

#endif // __BIGINT_HPP__
