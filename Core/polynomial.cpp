#include "polynomial.h"
#include "arithmeticexception.h"
#include "utility.h"
#include <cmath>
#include <cstring>
#include <iostream>


namespace Zigurat
{

  const int64_t Polynomial::BASE = 0x0100000000;

  const size_t Polynomial::KARATSUBA_THRESHOLD = 128;
  const size_t Polynomial::RECURSIVE_THRESHOLD = 256;

  const Polynomial Polynomial::BETA ({1, 0});
  const Polynomial Polynomial::ZERO ({0});
  const Polynomial Polynomial::ONE  ({1});
  const Polynomial Polynomial::TWO  ({2});
  const Polynomial Polynomial::THREE({3});

  Polynomial::Polynomial()
    : _data(nullptr), _length(0)
  {

  }

  Polynomial::Polynomial(const word_t* data, int length)
  {
    this->_data = new word_t[length];
    std::memcpy(this->_data, data, sizeof(word_t) * length);
    this->_length = length;
  }

  Polynomial::Polynomial(std::initializer_list<word_t> list)
    : _data(new word_t[list.size()]), _length(list.size())
  {
    int i = this->_length;
    for (const word_t& item : list)
      this->_data[--i] = item; 
  }

  Polynomial::Polynomial(size_t length)
    : _data(new word_t[length]), _length(length)
  {

  }

  Polynomial::Polynomial(size_t length, word_t value)
    : _data(new word_t[length]), _length(length)
  {
    this->fill(value);
  }

  Polynomial::Polynomial(Polynomial&& other)
    : _data(other._data), _length(other._length)
  {
    other._data   = nullptr;
    other._length = 0;
  }

  Polynomial::Polynomial(const Polynomial& other)
    : _data(new word_t[other._length]), _length(other._length)
  {
    std::memcpy(this->_data, other._data, sizeof(word_t) * other._length);
  }

  Polynomial& Polynomial::operator=(Polynomial&& other)
  {
    if (this->_data != nullptr) delete[] this->_data;

    this->_data   = other._data;
    this->_length = other._length;

    other._data   = nullptr;
    other._length = 0;

    return *this;
  }
  
  Polynomial& Polynomial::operator=(const Polynomial& other)
  {
    if (this->_data != nullptr) delete[] this->_data;

    this->_data   = new word_t[other._length];
    std::memcpy(this->_data, other._data, sizeof(word_t) * other._length);
    this->_length = other._length;

    return *this;
  }

  Polynomial Polynomial::from_uint64(uint64_t value)
  {
    return Polynomial({(word_t)(value >> 32), (word_t)(value & 0xFFFFFFFF)});
  }

  const Polynomial::word_t* Polynomial::data() const
  {
    return this->_data;
  }

  Polynomial::word_t* Polynomial::data()
  {
    return this->_data;
  }

  void Polynomial::fill(word_t value)
  {
    for (size_t i = 0; i < this->_length; i++)
      this->_data[i] = value;
  }

  Polynomial::word_t Polynomial::at(size_t index, word_t value) const
  {
    if (index < this->_length)
      return this->_data[index];
    return value;
  }

  const Polynomial::word_t& Polynomial::operator[](size_t index) const
  {
    return this->_data[index];
  }

  Polynomial::word_t& Polynomial::operator[](size_t index)
  {
    return this->_data[index];
  }

  size_t Polynomial::length() const
  {
    if (this->_length > 0) {
      for (size_t i = this->_length - 1; ; i--) {
	if (this->_data[i] != 0) 
	  return i + 1;
	if (i == 0) 
	  break;
      }
    }
    return 0;
  }

  bool Polynomial::is_zero() const
  {
    return (this->length() == 0);
  }

  bool Polynomial::is_odd() const
  {
    if (this->length() == 0) return false;
    return this->_data[0] & 0x01;
  }

  bool Polynomial::is_even() const
  {
    return !this->is_odd();
  }

  int Polynomial::cmp(const Polynomial& lhs, const Polynomial& rhs)
  {
    size_t m = lhs.length(), n = rhs.length();
    if (m < n) {
      return -1;
    } else if (m == n) {
      if (m > 0) {
	for (size_t i = m - 1; ; i--) {
	  if (lhs[i] < rhs[i]) return -1;
	  if (lhs[i] > rhs[i]) return 1;
	  if (i == 0) break;
	}
      }
    } else {
      return 1;
    }
    return 0;
  }

  Polynomial Polynomial::_naive_mul(const Polynomial& lhs, const Polynomial& rhs, size_t m, size_t n)
  {
    Polynomial result(m + n, 0);

    uint64_t sum = 0, carry = 0;
    for (size_t i = 0; i < m; i++) {
      for (size_t j = 0; j < n; j++) {
	sum = ((uint64_t)lhs._data[i] * rhs._data[j]) + result[i + j] + carry;
	carry = sum / BASE;
        result[i + j] = sum % BASE;
      }
      result[n + i] = carry;
      carry = 0;
    }

    return result;
  }

  Polynomial Polynomial::_karatsuba_mul(const Polynomial& lhs, const Polynomial& rhs, size_t n)
  {
    if (n < KARATSUBA_THRESHOLD) return Polynomial::_naive_mul(lhs, rhs, n, n);

    size_t k1 = n / 2, k0 = n - k1;

    Polynomial x1(lhs._data + k0, k1), x0(lhs._data, k0);
    Polynomial y1(rhs._data + k0, k1), y0(rhs._data, k0);
 
    Polynomial z2 = Polynomial::_karatsuba_mul(x1, y1, k1);
    Polynomial z0 = Polynomial::_karatsuba_mul(x0, y0, k0);
    Polynomial z1 = ((x1 + x0) * (y1 + y0)) - z2 - z0;

    return (z2 << (k0 * 2)) + (z1 << k0) + z0;
  }

  Polynomial Polynomial::_naive_div(const Polynomial& lhs, const Polynomial& rhs, size_t m, size_t n, Polynomial* rem)
  {
    Polynomial quot(m + 1, 0);
    Polynomial lhsn = lhs;
    Polynomial rhsn = rhs << m;

    if (lhsn >= rhsn) {
      quot[m] = 0x01;
      lhsn    = lhsn - rhsn;
    }

    uint64_t   qj = 0;
    size_t     l  = 0;
    Polynomial tmp;

    for (int j = m - 1; j >= 0; j--) {
      
      l = lhsn.length();

      if (l < n + j) {
	continue;
      } else if (l == 1) {
	qj = (uint64_t)lhsn[l - 1] / rhs[n - 1];
      } else {
	qj = (((uint64_t)lhsn[l - 1] * BASE) + lhsn[l - 2]) / rhs[n - 1];
      }
      quot[j] = Utility::min(qj, (uint64_t)BASE - 1);

      rhsn = rhs << j;
      tmp  = rhsn * Polynomial::from_uint64(quot[j]);
      while (lhsn < tmp) {
	quot[j] -= 1;
	tmp      = tmp - rhsn;
      }
      lhsn = lhsn - tmp;

    }

    if (rem != nullptr) *rem = lhsn;
    return quot;
  }

  Polynomial Polynomial::_recursive_div(const Polynomial& lhs, const Polynomial& rhs, size_t m, size_t n, Polynomial* rem)
  {
    if (m < RECURSIVE_THRESHOLD)
      return Polynomial::_naive_div(lhs, rhs, m, n, rem);

    size_t k = m / 2;

    Polynomial a1 = lhs >> 2 * k;
    Polynomial a0(lhs._data, 2 * k);
    Polynomial b1 = rhs >> k;
    Polynomial b0(rhs._data, k);
    
    Polynomial r1;
    Polynomial q1 = Polynomial::_recursive_div(a1, b1, k, k, &r1);
    
    Polynomial ap = (r1 << (2 * k)) + a0;
    Polynomial tmp = (q1 * b0) << k;
    while (ap < tmp) {
      q1 = q1 - Polynomial::ONE;
      tmp = tmp - (rhs << k);
    }
    ap = ap - tmp;

    Polynomial r0;
    Polynomial q0 = Polynomial::_recursive_div(ap >> k, b1, k, k, &r0);

    a0 = Polynomial(ap._data, k);
    ap = (r0 << k) + a0;
    tmp = (q0 * b0);
    while (ap < tmp) {
      q0 = q0 - Polynomial::ONE;
      tmp = tmp - rhs;
    }
    ap = ap - tmp;

    if (rem != nullptr) *rem = ap;

    return (q1 << k) + q0;
  }

  Polynomial Polynomial::div(const Polynomial& lhs, const Polynomial& rhs, Polynomial* rem)
  {
    const size_t m = lhs.length(), n = rhs.length(), l = m - n;

    if (m == 0) {
      if (rem != nullptr) rem->fill(0);
      return Polynomial();
    } else if (n == 0) {
      throw ArithmeticException("divide by zero");
    } else if (lhs < rhs) {
      if (rem != nullptr) *rem = lhs;
      return Polynomial();
    } else if (rhs[n - 1] < Polynomial::BASE / 2) { // Normalization
      Polynomial coef = Polynomial::from_uint64((uint64_t)std::ceil((double)BASE / 2 / rhs[n - 1]));
      Polynomial quot = Polynomial::div(lhs * coef, rhs * coef, rem);
      if (rem != nullptr) *rem = Polynomial::div(*rem, coef, nullptr);
      return quot;
    } else if (l < RECURSIVE_THRESHOLD) {
      return Polynomial::_naive_div(lhs, rhs, l, n, rem);
    } else if (n < l) { // Normalization
      Polynomial r;
      Polynomial quot = Polynomial::_recursive_div(lhs, rhs << (l - n), m - l, l, &r);
      if (r < rhs) {
	if (rem != nullptr) *rem = r;
	return quot << (l - n);
      } else {
	return (quot << (l - n)) + Polynomial::div(r, rhs, rem);
      }
    } else {
      return Polynomial::_recursive_div(lhs, rhs, m, n, rem);
    }
  }

  Polynomial Polynomial::pow(const Polynomial& base, const Polynomial& exponent)
  {
    Polynomial x = base, n = exponent, t;
    Polynomial out = Polynomial::ONE;

    while (true) {
      n = Polynomial::div(n, Polynomial::TWO, &t);
      if (t == Polynomial::ONE) out = out * x;
      if (n == Polynomial::ZERO)	break;
      x = x * x;
    }

    return out;
  }

  Polynomial Polynomial::mod_pow(const Polynomial& base, const Polynomial& exponent, const Polynomial& divisor)
  {
    if (exponent.is_zero()) return Polynomial::ONE;

    Polynomial remainder;
    if (exponent == Polynomial::ONE) {
      Polynomial::div(base, divisor, &remainder);
      return remainder;
    }

    Polynomial tmp;
    tmp = Polynomial::mod_pow(base, Polynomial::div(exponent, Polynomial::TWO, nullptr), divisor);
    tmp = tmp * tmp;
    if (exponent.is_odd() & 0x01) {
      tmp = tmp * base;
    }
    Polynomial::div(tmp, divisor, &remainder); 
    return remainder;
  }

  bool Polynomial::operator==(const Polynomial& other) const
  {
    return (Polynomial::cmp(*this, other) == 0);
  }

  bool Polynomial::operator!=(const Polynomial& other) const
  {
    return (Polynomial::cmp(*this, other) != 0);
  }

  bool Polynomial::operator<(const Polynomial& other) const
  {
    return (Polynomial::cmp(*this, other) == -1);
  }

  bool Polynomial::operator>(const Polynomial& other) const
  {
    return (Polynomial::cmp(*this, other) == 1);
  }

  bool Polynomial::operator<=(const Polynomial& other) const
  {
    return (Polynomial::cmp(*this, other) != 1);
  }

  bool Polynomial::operator>=(const Polynomial& other) const
  {
    return (Polynomial::cmp(*this, other) != -1);
  }

  Polynomial Polynomial::operator<<(const size_t count) const
  {
    if (count < 0) throw "count underflow";
    size_t length = this->_length + count;
    word_t array[length];
    std::memset(array, 0x00, sizeof(word_t) * count);
    std::memcpy(array + count, this->_data, sizeof(word_t) * this->_length);
    return Polynomial(array, length);
  }

  Polynomial Polynomial::operator>>(const size_t count) const
  {
    if (this->_length > count) {
      size_t length = this->_length - count;
      word_t array[length];
      std::memcpy(array, this->_data + count, sizeof(word_t) * length);
      return Polynomial(array, length);
    } else {
      return Polynomial();
    }
  }

  Polynomial Polynomial::operator+(const Polynomial& other) const
  {
    size_t max = Utility::max(this->length(), other.length());
    Polynomial result(max + 1, 0);

    int64_t sum = 0;
    bool    carry = false;
    for (size_t i = 0; i < max; i++) {
      sum = (int64_t)this->at(i, 0) + other.at(i, 0) + carry;
      result[i] = sum % BASE;
      carry = (sum > BASE - 1);
    }
    if (carry) result[max] = carry;

    return result;
  }

  Polynomial Polynomial::operator-(const Polynomial& other) const
  {
    size_t max = Utility::max(this->length(), other.length());
    Polynomial result(max, 0);

    int64_t sum = 0;
    bool    borrow = false;
    for (size_t i = 0; i < max; i++) {
      sum = (int64_t)this->at(i, 0) - other.at(i, 0) - borrow;
      result[i] = (sum < 0) ? BASE + sum : sum % BASE;
      borrow = (sum < 0);
    }
    if (borrow) throw ArithmeticException("subtraction underflow");

    return result;
  }

  Polynomial Polynomial::operator*(const Polynomial& other) const
  {
    const size_t m = this->length(), n = other.length();

    if (m == 0 || n == 0) {
      return Polynomial();
    } else if (m < KARATSUBA_THRESHOLD || n < KARATSUBA_THRESHOLD) {
      return Polynomial::_naive_mul(*this, other, m, n);
    } else if (m < n) { // Normalization
      return Polynomial::_karatsuba_mul(*this << (n - m), other, n) >> (n - m);
    } else if (m > n) { // Normalization
      return Polynomial::_karatsuba_mul(*this, other << (m - n), m) >> (m - n);
    } else {
      return Polynomial::_karatsuba_mul(*this, other, n);
    }
  }

  Polynomial Polynomial::operator/(const Polynomial& other) const
  {
    return Polynomial::div(*this, other, nullptr);
  }

  Polynomial Polynomial::operator%(const Polynomial& other) const
  {
    Polynomial rem;
    Polynomial::div(*this, other, &rem);
    return rem;
  }

  void Polynomial::push_back(typename Polynomial::word_t value)
  {
    if (this->_data != nullptr) {
      word_t* data = new word_t[this->_length + 1];
      std::memcpy(data, this->_data, sizeof(word_t) * this->_length);
      data[this->_length] = value;
      delete[] this->_data;
      this->_data = data;
    } else {
      this->_data = new word_t[1];
      this->_data[0] = value;
    }
  }

  Polynomial::~Polynomial()
  {
    if (this->_data != nullptr) delete[] this->_data;
  }

}
