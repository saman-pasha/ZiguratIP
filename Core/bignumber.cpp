#include "bignumber.h"
#include "utility.h"
#include "arithmeticexception.h"
#include <iomanip>
#include <cstring>


namespace Zigurat
{

  const int64_t   BigNumber::BASE = 0x0100000000;
  const int64_t   BigNumber::KARATSUBA_THRESHOLD = 8;

  const BigNumber BigNumber::BETA ({BigNumber::BASE});
  const BigNumber BigNumber::ZERO ({0});
  const BigNumber BigNumber::ONE  ({1});
  const BigNumber BigNumber::TWO  ({2});
  const BigNumber BigNumber::THREE({3});

  BigNumber::BigNumber()
  {

  }

  BigNumber::BigNumber(uint64_t value)
    : _a(2)
  {    
    this->_a[0] = value & 0xFFFFFFFF;
    this->_a[1] = value >> 32;
  }

  BigNumber::BigNumber(const uint8_t* octet, size_t length)
    : _a(std::ceil((double)length / sizeof(word_t)))
  {
    word_t w = 0;
    for (size_t i = 0; i < length; i += sizeof(word_t)) {
      for (size_t j = 0; j < sizeof(word_t); j++) {
	w <<= 8;
	w |= octet[i + j];
      }
      this->_a[ ((length - i) / sizeof(word_t)) - 1 ] = w;
    }
  }

  BigNumber::BigNumber(typename BigNumber::array_t&& array)
    : _a(std::move(array))
  {

  }

  BigNumber::BigNumber(const typename BigNumber::array_t& array)
    : _a(array)
  {

  }

  BigNumber::BigNumber(BigNumber&& other)
    : _a(std::move(other._a))
  {

  }

  BigNumber::BigNumber(const BigNumber& other)
    : _a(other._a)
  {

  }

  BigNumber& BigNumber::operator=(BigNumber&& other)
  {
    this->_a = std::move(other._a);
    return *this;
  }

  BigNumber& BigNumber::operator=(const BigNumber& other)
  {
    this->_a = other._a;
    return *this;
  }

  bool BigNumber::sign() const
  {
    static const word_t neg_val = 0x01 << ((sizeof(word_t) * 8) - 1);
    size_t length = this->length();
    if (length > 0) return this->_a[length - 1] & neg_val;
    return false;
  }

  void BigNumber::sign(bool sign)
  {
    static const word_t neg_val = 0x01 << ((sizeof(word_t) * 8) - 1);
    size_t length = this->length();
    if (length > 0) {
      if (sign) {
	if (this->_a[length - 1] < neg_val) 
	  this->_a[length - 1] |= neg_val;
	else 
	  this->_a.push_back(neg_val);
      } else {
	if (this->_a[length - 1] >= neg_val) this->_a.push_back(0x00);
      }
    }
  }

  bool BigNumber::is_odd() const
  {
    return this->_a.is_odd();
  }

  bool BigNumber::is_even() const
  {
    return !this->is_odd();
  }

  bool BigNumber::is_zero() const
  {
    return this->_a.is_zero();
  }

  size_t BigNumber::length() const
  {
    return this->_a.length();
  }

  void BigNumber::to_octet_string(uint8_t* octet, size_t length) const
  {
    size_t size = length / sizeof(word_t);
    std::memset(octet, 0x00, length);
    word_t w;
    for (int64_t i = size - 1; i >= 0; i--) {
      w = this->_a[i];
      for (size_t j = 0; j < sizeof(word_t); j++) {
        octet[ ((size - i - 1) * sizeof(word_t)) + sizeof(word_t) - j - 1] = w & 0xff;
	w >>= 8;
      }
    }
  }

  void BigNumber::to_octet_string(Buffer& stream) const
  {
    size_t size = this->length();
    word_t w;
    uint8_t buffer[sizeof(word_t)];
    for (int64_t i = size - 1; i >= 0 ; i--) {
      w = this->_a[i];
      for (size_t j = 0; j < sizeof(word_t); j++) {
	buffer[sizeof(word_t) - j - 1] = w & 0xff;
	w >>= 8;
      }
      stream.write(buffer, sizeof(word_t));
    }
  }

  int BigNumber::cmp(const BigNumber& lhs, const BigNumber& rhs)
  {
    if (lhs._sign && rhs._sign) {
      return -Polynomial::cmp(lhs._a, rhs._a);
    } else if (lhs._sign && !rhs._sign) {
      return -1;
    } else if (!lhs._sign && rhs._sign) {
      return 1;
    } else {
      return Polynomial::cmp(lhs._a, rhs._a);
    }
  }

  BigNumber BigNumber::div(const BigNumber& lhs, const BigNumber& rhs, BigNumber* rem)
  {
    const size_t n = rhs.length();
    const size_t m = lhs.length() - n;

    if (n == 0) {
      throw ArithmeticException("divide by zero");
    } else if (lhs.length() == 0) {
      if (rem != nullptr) rem->_a.fill(0);
      return array_t();
    } else if (lhs < rhs) {
      if (rem != nullptr) *rem = lhs;
      return array_t();
    } else if (rhs._a[n - 1] < BigNumber::BASE / 2) { // Normalization
      BigNumber coef = (uint64_t)std::ceil((double)BASE / 2 / rhs._a[n - 1]);
      BigNumber lhsn = lhs * coef;
      BigNumber rhsn = rhs * coef;
      BigNumber quot = BigNumber::div(lhsn, rhsn, rem);
      if (rem != nullptr) *rem = BigNumber::div(*rem, coef, nullptr);
      return quot;
    }

    array_t quot(m + 1, 0);
    BigNumber lhsn = lhs;
    BigNumber rhsn = rhs << m;

    if (lhsn >= rhsn) {
      quot[m] = 0x01;
      lhsn    = lhsn - rhsn;
    }

    uint64_t  qj = 0;
    size_t    l  = 0;
    BigNumber tmp;

    for (int64_t j = m - 1; j >= 0; j--) {
      
      l = lhsn.length();

      if (l < n + j) {
	continue;
      } else if (l == 1) {
	qj = (uint64_t)lhsn._a[l - 1] / rhs._a[n - 1];      
      } else {
	qj = (((uint64_t)lhsn._a[l - 1] * BASE) + lhsn._a[l - 2]) / rhs._a[n - 1];
      }
      quot[j] = Utility::min(qj, (uint64_t)BASE - 1);

      rhsn = rhs << j;
      tmp  = rhsn * quot[j];
      while (lhsn < tmp) {
	quot[j] -= 1;
	tmp      = tmp - rhsn;
      }
      lhsn = lhsn - tmp;

    }

    if (rem != nullptr) *rem = lhsn;
    return quot;
  }

  bool BigNumber::operator==(const BigNumber& other) const
  {
    return (BigNumber::cmp(*this, other) == 0);
  }

  bool BigNumber::operator!=(const BigNumber& other) const
  {
    return (BigNumber::cmp(*this, other) != 0);
  }

  bool BigNumber::operator<(const BigNumber& other) const
  {
    return (BigNumber::cmp(*this, other) == -1);
  }

  bool BigNumber::operator>(const BigNumber& other) const
  {
    return (BigNumber::cmp(*this, other) == 1);
  }

  bool BigNumber::operator<=(const BigNumber& other) const
  {
    return (BigNumber::cmp(*this, other) != 1);
  }

  bool BigNumber::operator>=(const BigNumber& other) const
  {
    return (BigNumber::cmp(*this, other) != -1);
  }

  BigNumber BigNumber::operator<<(const int count) const
  {
    return BigNumber(this->_a << count);
  }

  BigNumber BigNumber::operator>>(const int count) const
  {
    return BigNumber(this->_a >> count);
  }

  BigNumber BigNumber::operator+(const BigNumber& other) const
  {
    size_t  max = Utility::max(this->length(), other.length());
    array_t result(max + 1, 0);

    int64_t sum = 0;
    bool    carry = false;
    for (size_t i = 0; i < max; i++) {
      sum = (int64_t)this->_a.at(i, 0) + other._a.at(i, 0) + carry;
      result[i] = sum % BASE;
      carry = (sum > BASE - 1);
    }
    if (carry) result[max] = carry;

    return result;
  }

  BigNumber BigNumber::operator-(const BigNumber& other) const
  {
    size_t  max = Utility::max(this->length(), other.length());
    array_t result(max, 0);

    int64_t sum = 0;
    bool    borrow = false;
    for (size_t i = 0; i < max; i++) {
      sum = (int64_t)this->_a.at(i, 0) - other._a.at(i, 0) - borrow;
      result[i] = (sum < 0) ? BASE + sum : sum % BASE;
      borrow = (sum < 0);
    }
    if (borrow) throw ArithmeticException("subtraction underflow");

    return result;
  }

  BigNumber BigNumber::operator*(const BigNumber& other) const
  {
    size_t m = this->length(), n = other.length();
    array_t result(m + n, 0);

    uint64_t sum = 0, carry = 0;
    for (size_t i = 0; i < m; i++) {
      for (size_t j = 0; j < n; j++) {
	sum = ((uint64_t)this->_a[i] * other._a[j]) + result[i + j] + carry;
	carry = sum / BASE;
        result[i + j] = sum % BASE;
      }
      result[n + i] = carry;
      carry = 0;
    }

    return result;
  }

  BigNumber BigNumber::operator/(const BigNumber& other) const
  {
    return BigNumber::div(*this, other, nullptr);
  }

  BigNumber BigNumber::operator%(const BigNumber& other) const
  {
    BigNumber rem;
    BigNumber::div(*this, other, &rem);
    return rem;
  }

  BigNumber BigNumber::pow(const BigNumber& lhs, const BigNumber& rhs)
  {
    BigNumber x = lhs, n = rhs, t;
    BigNumber out = BigNumber::ONE;

    while (true) {
      n = BigNumber::div(n, BigNumber::TWO, &t);
      if (t == BigNumber::ONE) out = out * x;
      if (n == BigNumber::ZERO)	break;
      x = x * x;
    }

    return out;
  }

  BigNumber BigNumber::mod_pow(const BigNumber& base, const BigNumber& exponent, const BigNumber& divisor)
  {
    if (exponent.length() == 0) return BigNumber::ONE;

    BigNumber remainder;
    if (exponent == BigNumber::ONE) {
      BigNumber::div(base, divisor, &remainder);
      return remainder;
    }

    BigNumber tmp;
    tmp = BigNumber::mod_pow(base, BigNumber::div(exponent, BigNumber::TWO, nullptr), divisor);
    tmp = tmp * tmp;
    if (exponent.is_odd() & 0x01) {
      tmp = tmp * base;
    }
    BigNumber::div(tmp, divisor, &remainder); 
    return remainder;
  }

  BigNumber BigNumber::rand(size_t count)
  {
    static bool seeded = false;
    if (!seeded) {
      std::srand(std::time(0));
      seeded = true;
    }
    array_t vector(count);
    for (size_t i = 0; i < count - 1; i++) {
      vector[i] = std::rand() % BASE;
    }
    vector[count - 1] = std::rand() % (BASE / 2);
    return vector;
  }

  BigNumber BigNumber::rand(BigNumber r, BigNumber s)
  {
    BigNumber num = BigNumber::rand(r._a.length());
    while (num > s || num < r) {
      num = (num % s) + r;
    }
    return num;
  }

  BigNumber BigNumber::gcd(BigNumber m, BigNumber n)
  {
    if (m < n) return BigNumber::gcd(n, m);
    BigNumber r = m % n;
    if (r.is_zero()) return n;
    return BigNumber::gcd(n, r);
    /*
    if (u == v)
        return u;

    if (u == 0)
        return v;

    if (v == 0)
        return u;

    if (u.is_even())
    {
      if (v.is_odd())
	return BigNumber::gcd(u >> 1, v);
      else
	return BigNumber::gcd(u >> 1, v >> 1) << 1;
    }

    if (v.is_even())
      return BigNumber::gcd(u, v >> 1);

    if (u > v)
      return BigNumber::gcd((u - v) >> 1, v);

    return BigNumber::gcd((v - u) >> 1, u);
    */
  }

  void BigNumber::gcd(BigNumber a, BigNumber b, BigNumber& g, BigNumber& d)
  {
    d = 0;
    while (a.is_even() && b.is_even()) {
      a = a / 2;
      b = b / 2;
      d = d + 1;
    }
    while (a != b) {
      if (a.is_even()) a = a / 2;
      else if (b.is_even()) b = b / 2;
      else if (a > b) a = (a - b) / 2;
      else b = (b - a) / 2;
    }
    g = a;
  }

  BigNumber BigNumber::lcm(BigNumber a, BigNumber b)
  {
    return (a * b) / BigNumber::gcd(a, b);
  }

  BigNumber BigNumber::eed(BigNumber c, BigNumber d, BigNumber& a, BigNumber& b)
  {
    a = 0;
    b = 1;
    BigNumber aP = 1, bP = 0;
    BigNumber q, r, t;
    do {
      q = c / d;
      r = c % d;
      
      if (r.is_zero())
	break;

      c  = d;
      d  = r;
      t  = aP;
      aP = a;
      a  = t - (q * a);
      t  = bP;
      bP = b;
      b  = t - (q * b);
    } while (true);
    return d;
  }

  void BigNumber::print(std::ostream& ostream)
  {
    size_t len = this->length();
    for (int64_t i = len - 1; i > 0; i--) {
      ostream << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << this->_a[i] << ":";
    }
    if (len > 0)
      ostream << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << this->_a[0];
  }

}
