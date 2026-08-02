#include "array.h"
#include <cstdint>
#include <cstring>


namespace Zigurat
{

  template <typename T>
  Array<T>::Array()
    : _data(nullptr), _length(0)
  {

  }

  template <typename T>
  Array<T>::Array(const T* data, int length)
  {
    this->_data = new T[length];
    std::memcpy(this->_data, data, sizeof(T) * length);
    this->_length = length;
  }

  template <typename T>
  Array<T>::Array(std::initializer_list<T> list)
    : _data(new T[list.size()]), _length(list.size())
  {
    int i = this->_length;
    for (const T& item : list)
      this->_data[--i] = item; 
  }

  template <typename T>
  Array<T>::Array(int length)
    : _data(new T[length]), _length(length)
  {

  }

  template <typename T>
  Array<T>::Array(int length, T value)
    : _data(new T[length]), _length(length)
  {
    this->fill(value);
  }

  template <typename T>
  Array<T>::Array(Array<T>&& other)
    : _data(other._data), _length(other._length)
  {
    other._data   = nullptr;
    other._length = 0;
  }

  template <typename T>
  Array<T>::Array(const Array<T>& other)
    : _data(new T[other._length]), _length(other._length)
  {
    std::memcpy(this->_data, other._data, sizeof(T) * other._length);
  }

  template <typename T>
  Array<T>& Array<T>::operator=(Array<T>&& other)
  {
    if (this->_data != nullptr) delete[] this->_data;

    this->_data   = other._data;
    this->_length = other._length;

    other._data   = nullptr;
    other._length = 0;

    return *this;
  }
  
  template <typename T>
  Array<T>& Array<T>::operator=(const Array<T>& other)
  {
    if (this->_data != nullptr) delete[] this->_data;

    this->_data   = new T[other._length];
    std::memcpy(this->_data, other._data, sizeof(T) * other._length);
    this->_length = other._length;

    return *this;
  }

  template <typename T>
  const T* Array<T>::data() const
  {
    return this->_data;
  }

  template <typename T>
  T* Array<T>::data()
  {
    return this->_data;
  }

  template <typename T>
  void Array<T>::fill(T value)
  {
    for (int i = 0; i < this->_length; i++)
      this->_data[i] = value;
  }

  template <typename T>
  int Array<T>::length() const
  {
    return this->_length;
  }

  template <typename T>
  T Array<T>::at(int index, T value) const
  {
    if (index < this->_length)
      return this->_data[index];
    return value;
  }

  template <typename T>
  const T& Array<T>::operator[](int index) const
  {
    return this->_data[index];
  }

  template <typename T>
  T& Array<T>::operator[](int index)
  {
    return this->_data[index];
  }

  template <typename T>
  Array<T> Array<T>::operator<<(const int count) const
  {
    int length = this->_length + count;
    word_t array[length];
    std::memcpy(array, this->_data, sizeof(T) * this->_length);
    std::memset(array + this->_length, 0x00, sizeof(T) * count);
    return Array<T>(array, length);;
  }

  template <typename T>
  Array<T> Array<T>::operator>>(const int count) const
  {
    int length = this->_length - count;
    word_t array[length];
    std::memcpy(array, this->_data, sizeof(T) * length);
    return Array<T>(array, length);
  }

  template <typename T>
  Array<T>::~Array()
  {
    if (this->_data != nullptr) delete[] this->_data;
  }

  template class Array<uint8_t>;
  template class Array<uint32_t>;

}
