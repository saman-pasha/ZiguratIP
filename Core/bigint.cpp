#include "bigint.hpp"
#include "utility.hpp"
#include "arithmeticexception.hpp"
#include <iomanip>
#include <cstring>
#include <limits>
#include "bufferstream.hpp"


namespace Zigurat
{

  const typename BigInt::word_t  BigInt::POS_VAL = std::numeric_limits<word_t>::max() >> 1;
  const typename BigInt::word_t  BigInt::NEG_VAL = 0x01 << ((sizeof(word_t) * 8) - 1);

  BigInt::BigInt()
    : _a(), _sign(false)
  {

  }

  BigInt::BigInt(int32_t value)
    : _a(Polynomial::from_uint64(value))
  {
    if (this->_a[0] & NEG_VAL) {
      this->_a[0] &= POS_VAL;
      this->_sign = true;
    } else {
      this->_sign = false;
    }
  }

  BigInt::BigInt(uint32_t value)
    : _a(Polynomial::from_uint64(value))
  {
    this->_sign = false;
  }

  BigInt::BigInt(int64_t value)
    : _a(Polynomial::from_uint64(value))
  {
    if (this->_a[1] & NEG_VAL) {
      this->_a[1] &= POS_VAL;
      this->_sign = true;
    } else {
      this->_sign = false;
    }
  }

  BigInt::BigInt(uint64_t value)
    : _a(Polynomial::from_uint64(value))
  {
    this->_sign = false;
  }

  // Big-endian octets of any length. The words are filled from the least
  // significant end, so a 3 or 257 octet integer is as good as a 4 octet one:
  // this used to read whole words from the front, which walked off the end of a
  // short array and silently dropped whatever did not fill the last word. DER
  // integers are minimally encoded and rarely a multiple of the word size, so
  // nothing decoded from one could be trusted until this did.
  BigInt::BigInt(const uint8_t* octet, size_t length, bool sign)
    : _a((int)((length + sizeof(word_t) - 1) / sizeof(word_t)))
  {
    const size_t words = (length + sizeof(word_t) - 1) / sizeof(word_t);

    for (size_t i = 0; i < words; i++) {
      const size_t end = length - (i * sizeof(word_t));
      const size_t begin = (end > sizeof(word_t)) ? end - sizeof(word_t) : 0;

      word_t w = 0;
      for (size_t j = begin; j < end; j++) {
	w <<= 8;
	w |= octet[j];
      }
      this->_a[i] = w;
    }

    if (words > 0 && sign && (this->_a[words - 1] & NEG_VAL)) {
      this->_a[words - 1] &= POS_VAL;
      this->_sign = true;
    } else {
      this->_sign = false;
    }
  }

  BigInt::BigInt(typename BigInt::array_t&& array, bool sign)
    : _a(std::move(array)), _sign(sign)
  {

  }

  BigInt::BigInt(const typename BigInt::array_t& array, bool sign)
    : _a(array), _sign(sign)
  {

  }

  BigInt::BigInt(BigInt&& other)
    : _a(std::move(other._a)), _sign(other._sign)
  {

  }

  BigInt::BigInt(const BigInt& other)
    : _a(other._a), _sign(other._sign)
  {

  }

  BigInt& BigInt::operator=(BigInt&& other)
  {
    this->_a = std::move(other._a);
    this->_sign = other._sign;
    return *this;
  }

  BigInt& BigInt::operator=(const BigInt& other)
  {
    this->_a = other._a;
    this->_sign = other._sign;
    return *this;
  }

  bool BigInt::sign() const
  {
    if (this->is_zero()) return false;
    return this->_sign;
  }

  void BigInt::sign(bool sign)
  {
    this->_sign = sign;
  }

  bool BigInt::is_odd() const
  {
    return this->_a.is_odd();
  }

  bool BigInt::is_even() const
  {
    return !this->is_odd();
  }

  bool BigInt::is_zero() const
  {
    return this->_a.is_zero();
  }

  size_t BigInt::size(bool sign) const
  {
    size_t length = this->_a.length();
    if (length > 0 && sign && (this->_a[length - 1] & NEG_VAL)) return length + 1;
    return length;
  }

  size_t BigInt::length(bool sign) const
  {
    return this->size(sign) * sizeof(word_t);
  }

  void BigInt::to_octet_string(uint8_t* octet, bool sign) const
  {
    size_t a_len = this->_a.length();
    if (a_len > 0) {

      size_t size = a_len;
      word_t w = this->_a[a_len - 1];

      if (sign && (w & NEG_VAL)) {
	word_t val = (this->sign()) ? NEG_VAL : 0x00;
	for (size_t i = 0; i < sizeof(word_t); i++)
	  *octet++ = (val >> (sizeof(word_t) - i - 1));
	size++;
      }

      if (sign && this->sign() && !(w & NEG_VAL)) w |= NEG_VAL;

      for (size_t i = a_len; i > 0; i--) {
	for (size_t j = 0; j < sizeof(word_t); j++) {
	  octet[((a_len - i) * sizeof(word_t)) + j] = (w >> ((sizeof(word_t) - j - 1) * 8)) & 0xff;
	}
	// The last turn has no next word to fetch: i is 1, so i - 2 is
	// size_t(-1) and this read the array from an index near the top of the
	// address space. The value went unused -- the loop ends -- so nothing
	// ever looked wrong, but every RSA verify walked off the end of a
	// BigInt to get it.
	if (i >= 2) w = this->_a[i - 2];
      }
    }
  }

  void BigInt::to_octet_string(binarystream& stream, bool sign) const
  {
    size_t a_len = this->_a.length();
    if (a_len > 0) {

      size_t size = a_len;
      word_t w = this->_a[a_len - 1];

      if (sign && (w & NEG_VAL)) {
	word_t val = (this->sign()) ? NEG_VAL : 0x00;
	for (size_t i = 0; i < sizeof(word_t); i++)
	  stream.put(val >> (sizeof(word_t) - i - 1));
	size++;
      }

      if (sign && this->sign() && !(w & NEG_VAL)) w |= NEG_VAL;

      for (size_t i = a_len; i > 0; i--) {
	for (size_t j = 0; j < sizeof(word_t); j++) {
	  stream.put((w >> ((sizeof(word_t) - j - 1) * 8)) & 0xff);
	}
	if (i >= 2) w = this->_a[i - 2];   // see the octet overload above
      }
    }
  }

  int BigInt::cmp(const BigInt& lhs, const BigInt& rhs)
  {
    if (lhs.sign() && rhs.sign()) {
      return -Polynomial::cmp(lhs._a, rhs._a);
    } else if (lhs.sign() && !rhs.sign()) {
      return -1;
    } else if (!lhs.sign() && rhs.sign()) {
      return 1;
    } else {
      return Polynomial::cmp(lhs._a, rhs._a);
    }
  }

  BigInt BigInt::div(const BigInt& dividend, const BigInt& divisor, BigInt* remainder)
  {
    bool out_sign;
    if (dividend.sign() && divisor.sign()) {
      out_sign = false;
    } else if (dividend.sign() && !divisor.sign()) {
      out_sign = true;
    } else if (!dividend.sign() && divisor.sign()) {
      out_sign = true;
    } else {
      out_sign = false;
    }
    return BigInt(array_t::div(dividend._a, divisor._a, (remainder == nullptr) ? nullptr : &remainder->_a), out_sign);

    int64_t n = divisor._a.length();
    int64_t m = dividend._a.length() - n;

    if (n == 0) {
      throw ArithmeticException("divide by zero");
    } else if (dividend._a.length() == 0) {
      if (remainder != nullptr) remainder->_a.fill(0);
      return BigInt();
    } else if (dividend < divisor) {
      if (remainder != nullptr) *remainder = dividend;
      return BigInt();
    }

    BigInt quot;
    BigInt a = dividend;
    BigInt a1, a0;
    BigInt q, r;

    while (m > n) {
      a1 = a >> (m - n);
      a0 = BigInt(array_t(a._a.data(), (m - n)));
      
      q = array_t::div(a1._a, divisor._a, &r._a);
      
      quot = quot << n;
      quot = quot + q;
      
      r = r << (m - n);
      a = r + a0;
      
      m = m - n;
    }

    q = array_t::div(a._a, divisor._a, &r._a);
    quot = quot << m;
    quot = quot + q;

    if (remainder != nullptr) {
      *remainder = r;
    }

    quot.sign(out_sign);
    return quot;

    /*
    size_t n = divisor.size();
    size_t m = dividend.size() - n;

    if (m < RECURSIVE_THRESHOLD)
      return BigInt(array_t::div(dividend._a, divisor._a, &remainder->_a), out_sign);

    int k = std::floor((double)m / 2);
    BigInt a1 = dividend._a >> (2 * k);
    BigInt a0(array_t(dividend._a.data(), (2 *k)));
    BigInt b1 = divisor >> k;
    BigInt b0(array_t(divisor._a.data(), k));
    
    BigInt r1;
    BigInt q1 = BigInt::div(a1, b1, &r1);
    
    BigInt ap = (r1 << (2 * k)) + a0;
    ap = ap - ((q1 * b0) << k);
    while (BigInt::cmp(ap, array_t::ZERO) == -1) {
      q1 = q1 - array_t::ONE;
      ap = ap + (divisor << k);
    }

    a1 = ap >> k;
    a0 = BigInt(array_t(ap._a.data(), k));

    BigInt r0;
    BigInt q0 = BigInt::div(a1, b1, &r0);

    ap = (r0 << k) + a0;
    ap = ap - (q0 * b0);
    while (BigInt::cmp(ap, array_t::ZERO) == -1) {
      q0 = q0 - array_t::ONE;
      ap = ap + divisor;
    }

    q1 = q1 << k;
    q1 = q1 + q0;

    if (remainder != nullptr) {
      *remainder = ap;
    }

    return q1;
    */
  }

  bool BigInt::operator==(const BigInt& other) const
  {
    return (BigInt::cmp(*this, other) == 0);
  }

  bool BigInt::operator!=(const BigInt& other) const
  {
    return (BigInt::cmp(*this, other) != 0);
  }

  bool BigInt::operator<(const BigInt& other) const
  {
    return (BigInt::cmp(*this, other) == -1);
  }

  bool BigInt::operator>(const BigInt& other) const
  {
    return (BigInt::cmp(*this, other) == 1);
  }

  bool BigInt::operator<=(const BigInt& other) const
  {
    return (BigInt::cmp(*this, other) != 1);
  }

  bool BigInt::operator>=(const BigInt& other) const
  {
    return (BigInt::cmp(*this, other) != -1);
  }

  BigInt BigInt::operator<<(const size_t count) const
  {
    return BigInt(this->_a << count);
  }

  BigInt BigInt::operator>>(const size_t count) const
  {
    return BigInt(this->_a >> count);
  }

  BigInt BigInt::operator+(const BigInt& other) const
  {
    if (this->sign() && other.sign()) {
      return BigInt(this->_a + other._a, true);
    } else if (this->sign() && !other.sign()) {
      if (array_t::cmp(this->_a, other._a) == -1) 
	return BigInt(other._a - this->_a, false);
      else 
	return BigInt(this->_a - other._a, true);
    } else if (!this->sign() && other.sign()) {
      if (array_t::cmp(this->_a, other._a) == -1)
	return BigInt(other._a - this->_a, true);
      else
	return BigInt(this->_a - other._a, false);
    } else {
      return BigInt(this->_a + other._a, false);
    }    
  }

  BigInt BigInt::operator-(const BigInt& other) const
  {
    if (this->sign() && other.sign()) {
      if (array_t::cmp(this->_a, other._a) == -1)
	return BigInt(other._a - this->_a, false);
      else
	return BigInt(this->_a - other._a, true);
    } else if (this->sign() && !other.sign()) {
      return BigInt(this->_a + other._a, true);
    } else if (!this->sign() && other.sign()) {
      return BigInt(this->_a + other._a, false);
    } else {
      if (array_t::cmp(this->_a, other._a) == -1)
	return BigInt(other._a - this->_a, true);
      else
	return BigInt(this->_a - other._a, false);
    }
  }

  BigInt BigInt::operator*(const BigInt& other) const
  {
    bool out_sign;
    if (this->sign() && other.sign()) {
      out_sign = false;
    } else if (this->sign() && !other.sign()) {
      out_sign = true;
    } else if (!this->sign() && other.sign()) {
      out_sign = true;
    } else {
      out_sign = false;
    }
    return BigInt(this->_a * other._a, out_sign);
  }

  BigInt BigInt::operator/(const BigInt& other) const
  {
    return BigInt::div(*this, other, nullptr);
  }

  BigInt BigInt::operator%(const BigInt& other) const
  {
    BigInt remainder;
    BigInt::div(*this, other, &remainder);
    return remainder;
  }

  BigInt BigInt::pow(const BigInt& base, const BigInt& exponent)
  {
    return Polynomial::pow(base._a, exponent._a);
  }

  BigInt BigInt::mod_pow(const BigInt& base, const BigInt& exponent, const BigInt& divisor)
  {
    return Polynomial::mod_pow(base._a, exponent._a, divisor._a);
  }

  // This drew from std::rand(), seeded once from std::time(0). Two keys
  // generated in the same second were the same key, and a key generated at a
  // known minute was one of about sixty candidates -- which is every
  // certificate this tree has ever issued. They all have to be reissued;
  // fixing this does not repair the material it already produced.
  BigInt BigInt::rand(size_t count)
  {
    if (count == 0) throw ArithmeticException("random number of zero words requested");
    array_t vector(count);

    // Every word gets a full word of entropy. The old code said
    // std::rand() % BASE, but BASE is 2^32 and std::rand() tops out at 2^31-1,
    // so the high bit of every single word was always clear -- a bit lost per
    // word on top of the seeding.
    for (size_t i = 0; i < count; i++) {
      uint32_t word;
      Utility::random_bytes((uint8_t*)&word, sizeof(word));
      vector[i] = word;
    }

    // The top word carries the sign, so the value has to stay under BASE/2.
    // Within that, force the highest bit that is still positive: a caller
    // asking for `count` words wants a number of that size, and leaving the
    // top bit to chance is why a 2048-bit key from this tree measures 2047.
    vector[count - 1] = (vector[count - 1] & (Polynomial::BASE / 2 - 1)) | (Polynomial::BASE / 4);
    return vector;
  }

  BigInt BigInt::rand(BigInt r, BigInt s)
  {
    BigInt num = BigInt::rand(r._a.length());
    while (num > s || num < r) {
      num = (num % s) + r;
    }
    return num;
  }

  BigInt BigInt::gcd(const BigInt& m, const BigInt& n)
  {
    if (m < n) return BigInt::gcd(n, m);
    BigInt r = m % n;
    if (r.is_zero()) return n;
    return BigInt::gcd(n, r);
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
	return BigInt::gcd(u >> 1, v);
      else
	return BigInt::gcd(u >> 1, v >> 1) << 1;
    }

    if (v.is_even())
      return BigInt::gcd(u, v >> 1);

    if (u > v)
      return BigInt::gcd((u - v) >> 1, v);

    return BigInt::gcd((v - u) >> 1, u);
    */
  }

  void BigInt::gcd(const BigInt& a, const BigInt& b, BigInt& g, BigInt& d)
  {
    BigInt aP = a, bP = b;
    d = 0;
    while (aP.is_even() && bP.is_even()) {
      aP = aP / 2;
      bP = bP / 2;
      d = d + 1;
    }
    while (aP != bP) {
      if (aP.is_even()) aP = aP / 2;
      else if (bP.is_even()) bP = bP / 2;
      else if (aP > bP) aP = (aP - bP) / 2;
      else bP = (bP - aP) / 2;
    }
    g = aP;
  }

  BigInt BigInt::lcm(const BigInt& a, const BigInt& b)
  {
    return (a * b) / BigInt::gcd(a, b);
  }

  BigInt BigInt::eed(const BigInt& c, const BigInt& d, BigInt& a, BigInt& b)
  {
    BigInt cP = c, dP = d;
    a = 0;
    b = 1;
    BigInt aP = 1, bP = 0;
    BigInt q, r, t;
    do {
      q = BigInt::div(cP, dP, &r);
      
      if (r.is_zero())
	break;

      cP  = dP;
      dP  = r;
      t  = aP;
      aP = a;
      a  = t - (q * a);
      t  = bP;
      bP = b;
      b  = t - (q * b);
    } while (true);

    return dP;
  }

  BigInt BigInt::inverse(const BigInt& a, const BigInt& n)
  {
    BigInt t = 0, newt = 1, r = n, newr = a;
    BigInt tmp, quotient;
    while (!newr.is_zero()) {
      quotient = r / newr;

      tmp = t;
      t = newt;
      newt = tmp - (quotient * newt);

      tmp = r;
      r = newr;
      newr = tmp - (quotient * newr);
    }
    if (r > array_t::ONE) throw ArithmeticException("coefficient is not invertible");
    if (t < array_t::ZERO) t = t + n;
    return t;
  }

  void BigInt::print(std::ostream& ostream)
  {
    bufferstream buffer;
    this->to_octet_string(buffer, true);
    size_t length = buffer.length() - 1;
    for (size_t i = 0; i < length; i++) {
      ostream << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)buffer.get() << ":";    
    }
    ostream << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << (int)buffer.get();
  }

  std::string BigInt::to_string()
  {
    std::stringstream ss;
    this->print(ss);
    return ss.str();
  }

  BigInt::~BigInt() 
  {

  }

}
