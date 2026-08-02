
#ifndef __POLYNOMIAL_H__
#define __POLYNOMIAL_H__

#include <cstdint>
#include <cstddef>
#include <initializer_list>

namespace Zigurat
{

  class Polynomial
  {
  public:
    typedef uint32_t word_t;

    static const int64_t BASE;

    static const size_t KARATSUBA_THRESHOLD;
    static const size_t RECURSIVE_THRESHOLD;

    static const Polynomial BETA;
    static const Polynomial ZERO;
    static const Polynomial ONE;
    static const Polynomial TWO;
    static const Polynomial THREE;

  protected:
    word_t* _data;
    size_t  _length;

    static Polynomial _naive_mul(const Polynomial&, const Polynomial&, size_t, size_t);
    static Polynomial _karatsuba_mul(const Polynomial&, const Polynomial&, size_t);
    static Polynomial _naive_div(const Polynomial&, const Polynomial&, size_t, size_t, Polynomial*);
    static Polynomial _recursive_div(const Polynomial&, const Polynomial&, size_t, size_t, Polynomial*);

  public:
    Polynomial();
    Polynomial(const word_t*, int);
    Polynomial(std::initializer_list<word_t>);
    Polynomial(size_t);
    Polynomial(size_t, word_t);
    Polynomial(Polynomial&&);
    Polynomial(const Polynomial&);

    Polynomial& operator=(Polynomial&&);
    Polynomial& operator=(const Polynomial&);

    static Polynomial from_uint64(uint64_t);

    const word_t* data() const;
    word_t*       data();

    word_t        at(size_t, word_t) const;
    const word_t& operator[](size_t) const;
    word_t&       operator[](size_t);

    void   fill(word_t);
    size_t length()  const;
    bool   is_zero() const;
    bool   is_odd()  const;
    bool   is_even() const;

    static int cmp (const Polynomial&, const Polynomial&);
    static Polynomial div(const Polynomial&, const Polynomial&, Polynomial*);
    static Polynomial pow(const Polynomial&, const Polynomial&);
    static Polynomial mod_pow(const Polynomial&, const Polynomial&, const Polynomial&);

    bool operator==(const Polynomial&) const;
    bool operator!=(const Polynomial&) const;
    bool operator< (const Polynomial&) const;
    bool operator> (const Polynomial&) const;
    bool operator<=(const Polynomial&) const;
    bool operator>=(const Polynomial&) const;

    Polynomial operator<<(const size_t) const;
    Polynomial operator>>(const size_t) const;

    Polynomial operator+(const Polynomial&) const;
    Polynomial operator-(const Polynomial&) const;
    Polynomial operator*(const Polynomial&) const;
    Polynomial operator/(const Polynomial&) const;
    Polynomial operator%(const Polynomial&) const;

    void push_back(word_t);

    virtual ~Polynomial();
  };

}

#endif // __POLYNOMIAL_H__
