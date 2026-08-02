
#ifndef __VECTOR_HPP__
#define __VECTOR_HPP__

#include "typeobject.hpp"
#include "typeulong.hpp"
#include "typestring.hpp"
#include <sstream>
#include <vector>
#include "binarystream.hpp"

namespace Zigurat
{

  class String;

  template <typename T>
  class Vector : public Object
  {
  public:
    typedef std::vector<T> SCT;
    typedef typename T::SCT ELEMENT_SCT;
    static const uint8_t TDB;
    static const uint8_t ELEMENT_TDB;

    virtual uint8_t tdb() const override;

    Vector();
    Vector(std::nullptr_t);
    Vector(std::vector<T>&&);
    Vector(const std::vector<T>&);
    Vector(Vector<T>&&);
    Vector(const Vector<T>&);
    Vector(size_t);
    Vector(const std::vector<typename T::SCT>&);

    Vector<T>& operator=(std::nullptr_t);
    Vector<T>& operator=(std::vector<T>&&);
    Vector<T>& operator=(const std::vector<T>&);
    Vector<T>& operator=(Vector<T>&&);
    Vector<T>& operator=(const Vector<T>&);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(std::vector<T>&&) const;
    virtual bool operator==(const std::vector<T>&) const;
    virtual bool operator==(Vector<T>&&) const;
    virtual bool operator==(const Vector<T>&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(std::vector<T>&&) const;
    virtual bool operator!=(const std::vector<T>&) const;
    virtual bool operator!=(Vector<T>&&) const;
    virtual bool operator!=(const Vector<T>&) const;
    
    virtual bool operator<(const Vector<T>&) const;
    virtual bool operator<=(const Vector<T>&) const;
    virtual bool operator>(const Vector<T>&) const;
    virtual bool operator>=(const Vector<T>&) const;

    virtual void set_null();
    virtual const std::vector<T>& value() const;

    T& operator[](size_t);
    T& operator[](ULong);
    
    virtual size_t std_size();
    virtual ULong size();
    virtual ULong SIZE();

    virtual void std_resize(size_t);
    virtual void resize(ULong);
    virtual void RESIZE(ULong);

    virtual T GET(ULong) const;
    virtual void SET(ULong, const T);

    virtual int64_t pack_size() const override;
    virtual std::string to_std_string() const override;
    
    virtual ~Vector();

    friend binarystream& operator<<(binarystream& outstream, Vector<T>&& object)
    {
      outstream.write_std_ubyte(object.tdb());
      if (object._pointer != nullptr) {
        outstream.write_std_uint((uint32_t)object.size());
	for (size_t i = 0; i < object.size(); i++) {
	  outstream << object[i];
	}
      }
      return outstream;
    }
  
    friend binarystream& operator<<(binarystream& outstream, const Vector<T>& object)
    {
      outstream.write_std_ubyte(object.tdb());
      if (object._pointer != nullptr) {
        outstream.write_std_uint((uint32_t)object.size());
	for (size_t i = 0; i < object.size(); i++) {
	  outstream << object[i];
	}
      }
      return outstream;
    }
  
    friend binarystream& operator>>(binarystream& instream, Vector<T>& object)
    {
      uint8_t tdb = instream.read_std_ubyte();
      if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
	object.set_null();
      } else {
	if (object._pointer == nullptr) object._pointer = new std::vector<T>();
	uint32_t size = instream.read_std_uint();
	object.resize((size_t)size);
	for (size_t i = 0; i < size; i++) {
	  instream >> object[i];
	}
      }
      return instream;
    }

  };

  template <typename T>
  const uint8_t Vector<T>::TDB = TDByte::VECTOR | T::TDB;

  template <typename T>
  const uint8_t Vector<T>::ELEMENT_TDB = T::TDB;

  template <typename T>
  uint8_t Vector<T>::tdb() const
  {
    return (this->_pointer == nullptr) ? Vector<T>::TDB | TDByte::IS_NULL : Vector<T>::TDB;
  }

  template <typename T>
  Vector<T>::Vector()
  {
    this->_pointer = new std::vector<T>();
  }
  
  template <typename T>
  Vector<T>::Vector(std::nullptr_t)
  {

  }

  template <typename T>
  Vector<T>::Vector(std::vector<T>&& other)
  {
    this->_pointer = new std::vector<T>(std::move(other));
  }

  template <typename T>
  Vector<T>::Vector(const std::vector<T>& other)
  {
    this->_pointer = new std::vector<T>(other);
  }

  template <typename T>
  Vector<T>::Vector(Vector<T>&& other)
  {
    this->_pointer = other._pointer;
    other._pointer = nullptr;
  }

  template <typename T>
  Vector<T>::Vector(const Vector<T>& other)
  {
    if (other._pointer != nullptr)
      this->_pointer = new std::vector<T>(*(std::vector<T>*)other._pointer);
  }

  template <typename T>
  Vector<T>::Vector(size_t size)
  {
    this->_pointer = new std::vector<T>(size);
  }

  template <typename T>
  Vector<T>::Vector(const std::vector<typename T::SCT>& other)
  {
    this->_pointer = new std::vector<T>(other.size());
    for (size_t i = 0; i < other.size(); i++)
      (*(std::vector<T>*)this->_pointer)[i] = other[i];
  }

  template <typename T>
  Vector<T>& Vector<T>::operator=(std::nullptr_t)
  {
    this->set_null();
    return *this;
  }

  template <typename T>
  Vector<T>& Vector<T>::operator=(std::vector<T>&& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new std::vector<T>(std::move(other));
    }
    return *this;
  }
	
  template <typename T>
  Vector<T>& Vector<T>::operator=(const std::vector<T>& other)
  {
    if (this->_pointer != &other) {
      this->set_null();
      this->_pointer = new std::vector<T>(other);
    }
    return *this;
  }

  template <typename T>
  Vector<T>& Vector<T>::operator=(Vector<T>&& other)
  {
    if (this != &other) {
      this->set_null();
      this->_pointer = other._pointer;
      other._pointer = nullptr;
    }
    return *this;
  }
	
  template <typename T>
  Vector<T>& Vector<T>::operator=(const Vector<T>& other)
  {
    if (this != &other) {
      this->set_null();
      if (other._pointer != nullptr)
	this->_pointer = new std::vector<T>(*(std::vector<T>*)other._pointer);
    }
    return *this;
  }

  template <typename T>
  bool Vector<T>::operator==(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  template <typename T>
  bool Vector<T>::operator==(std::vector<T>&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer == other);
  }
	
  template <typename T>
  bool Vector<T>::operator==(const std::vector<T>& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer == other);
  }
	
  template <typename T>
  bool Vector<T>::operator==(Vector<T>&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer == *(std::vector<T>*)other._pointer);
  }
	
  template <typename T>
  bool Vector<T>::operator==(const Vector<T>& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer == *(std::vector<T>*)other._pointer);
  }
	
  template <typename T>
  bool Vector<T>::operator!=(std::nullptr_t) const
  {
    throw NULL_EXCEPTION;
  }
	
  template <typename T>
  bool Vector<T>::operator!=(std::vector<T>&& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer != other);
  }
	
  template <typename T>
  bool Vector<T>::operator!=(const std::vector<T>& other) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer != other);
  }
	
  template <typename T>
  bool Vector<T>::operator!=(Vector<T>&& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer != *(std::vector<T>*)other._pointer);
  }

  template <typename T>
  bool Vector<T>::operator!=(const Vector<T>& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer != *(std::vector<T>*)other._pointer);
  }
	
  template <typename T>
  bool Vector<T>::operator<(const Vector<T>& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer < *(std::vector<T>*)other._pointer);
  }
	
  template <typename T>
  bool Vector<T>::operator<=(const Vector<T>& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer <= *(std::vector<T>*)other._pointer);
  }
	
  template <typename T>
  bool Vector<T>::operator>(const Vector<T>& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer > *(std::vector<T>*)other._pointer);
  }
	
  template <typename T>
  bool Vector<T>::operator>=(const Vector<T>& other) const
  {
    if (this->_pointer == nullptr || other._pointer == nullptr) throw NULL_EXCEPTION;
    return (*(std::vector<T>*)this->_pointer >= *(std::vector<T>*)other._pointer);
  }

  template <typename T>
  void Vector<T>::set_null()
  {
    if (this->_pointer != nullptr) {
      delete (std::vector<T>*)this->_pointer;
      this->_pointer = nullptr;
    }
  }

  template <typename T>
  const std::vector<T>& Vector<T>::value() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return *(std::vector<T>*)this->_pointer;
  }
	
  template <typename T>
  size_t Vector<T>::std_size()
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return ((std::vector<T>*)this->_pointer)->size();
  }
 
  template <typename T>
  ULong Vector<T>::size()
  {
    return (uint64_t)this->std_size();
  }
 
  template <typename T>
  ULong Vector<T>::SIZE()
  {
    return (uint64_t)this->std_size();
  }

  template <typename T>
  void Vector<T>::std_resize(size_t size)
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    ((std::vector<T>*)this->_pointer)->resize(size);
  }

  template <typename T>
  void Vector<T>::resize(ULong size)
  {
    this->std_resize(size.value());
  }

  template <typename T>
  void Vector<T>::RESIZE(ULong size)
  {
    this->std_resize(size.value());
  }

  template <typename T>
  T Vector<T>::GET(ULong index) const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return T((*(std::vector<T>*)this->_pointer)[index.value()]);
  }

  template <typename T>
  void Vector<T>::SET(ULong index, const T value)
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    (*(std::vector<T>*)this->_pointer)[index.value()] = value.value();
  }

  template <typename T>
  int64_t Vector<T>::pack_size() const
  {
    return (this->_pointer == nullptr) ? 1 : 
      TDByte::SIZEOF_DWORD + (((std::string*)this->_pointer)->size() * TDByte::SIZEOF(T::TDB)) + 1;
  }

  template <typename T>
  std::string Vector<T>::to_std_string() const
  {
    if (this->_pointer == nullptr) return NULL_STRING;
    std::stringstream ss;
    for (size_t i = 0; i < ((std::vector<T>*)this->_pointer)->size(); i++) {
      ss << (*((std::vector<T>*)this->_pointer))[i].to_string().value();
    }
    return ss.str();
  }

  template <typename T>
  Vector<T>::~Vector()
  {
    this->set_null();
  }

}

#endif // __VECTOR_HPP__
