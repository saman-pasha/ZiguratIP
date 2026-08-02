
#ifndef __ARRAY_HPP__
#define __ARRAY_HPP__

#include <initializer_list>

namespace Zigurat
{

  template <typename T>
  class Array
  {
  public:
    typedef T word_t;

  protected:
    T*   _data;
    int  _length;

  public:
    Array();
    Array(const T*, int);
    Array(std::initializer_list<T>);
    Array(int);
    Array(int, T);
    Array(Array<T>&&);
    Array(const Array<T>&);

    Array<T>& operator=(Array<T>&&);
    Array<T>& operator=(const Array<T>&);

    virtual const T* data() const;
    virtual T*       data();

    virtual void fill(T);
    virtual int  length() const;

    virtual T        at(int, T) const;
    virtual const T& operator[](int) const;
    virtual T&       operator[](int);

    Array<T> operator<<(const int) const;
    Array<T> operator>>(const int) const;

    virtual ~Array();
  };

}

#endif // __ARRAY_HPP__
