
#ifndef __BIGNUMBER_H__
#define __BIGNUMBER_H__

#include "polynomial.h"
#include "buffer.h"
#include <iostream>

namespace Zigurat
{

  class BigNumber
  {
  public:
    typedef uint32_t   word_t;
    typedef Polynomial array_t;

  protected:
    static const int64_t   BASE;
    static const int64_t   KARATSUBA_THRESHOLD;

    static const BigNumber BETA;
    static const BigNumber ZERO;
    static const BigNumber ONE;
    static const BigNumber TWO;
    static const BigNumber THREE;

    array_t _a;
    bool    _sign;

  public:
    BigNumber();
    BigNumber(uint64_t);
    BigNumber(const uint8_t*, size_t);
    BigNumber(array_t&&);
    BigNumber(const array_t&);
    BigNumber(BigNumber&&);
    BigNumber(const BigNumber&);

    BigNumber& operator=(BigNumber&&);
    BigNumber& operator=(const BigNumber&);

    bool   sign() const;
    void   sign(bool);
    size_t length()  const;
    bool   is_zero() const;
    bool   is_odd()  const;
    bool   is_even() const;
    void   to_octet_string(uint8_t*, size_t) const;
    void   to_octet_string(Buffer&) const;

    static int cmp (const BigNumber&, const BigNumber&);
    static BigNumber div(const BigNumber&, const BigNumber&, BigNumber*);

    bool operator==(const BigNumber&) const;
    bool operator!=(const BigNumber&) const;
    bool operator< (const BigNumber&) const;
    bool operator> (const BigNumber&) const;
    bool operator<=(const BigNumber&) const;
    bool operator>=(const BigNumber&) const;

    BigNumber operator<<(const int) const;
    BigNumber operator>>(const int) const;

    BigNumber operator+(const BigNumber&) const;
    BigNumber operator-(const BigNumber&) const;
    BigNumber operator*(const BigNumber&) const;
    BigNumber operator/(const BigNumber&) const;
    BigNumber operator%(const BigNumber&) const;

    static BigNumber pow(const BigNumber&, const BigNumber&);                       // base ^ exponent
    static BigNumber mod_pow(const BigNumber&, const BigNumber&, const BigNumber&); // (base ^ exponent) mod divisor
    static BigNumber rand(size_t);                                                  // Random Polynomial Generation of the size
    static BigNumber rand(BigNumber, BigNumber);                                    // Random Polynomial Generation between r and s
    static BigNumber gcd(BigNumber, BigNumber);                                     // Greatest Common Divisor
    static void      gcd(BigNumber, BigNumber, BigNumber&, BigNumber&);             // g is odd and gcd(a, b) = g × (2**d)
    static BigNumber lcm(BigNumber, BigNumber);                                     // Least Common Multiple
    static BigNumber eed(BigNumber, BigNumber, BigNumber&, BigNumber&);             // Extended Euclidean Divisor

    void print(std::ostream&);

    friend std::ostream& operator<<(std::ostream& os, BigNumber num)
    {
      num.print(os);
      return os;
    }

  };

}

#endif // __BIGNUMBER_H__
