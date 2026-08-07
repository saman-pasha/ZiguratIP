
#ifndef __VECTOR_HPP__
#define __VECTOR_HPP__


#include <cstdint>
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
    
    virtual size_t std_size() const;
    virtual ULong size() const;
    virtual ULong SIZE();

    virtual void std_resize(size_t);
    virtual void resize(ULong);
    virtual void RESIZE(ULong);

    virtual T GET(ULong) const;
    virtual void SET(ULong, const T);

    virtual int64_t pack_size() const override;
    virtual std::string to_std_string() const override;
    
    virtual ~Vector();

    // NEITHER OF THESE HAD EVER BEEN INSTANTIATED, which is why they were full
    // of things that do not compile: (uint32_t)object.size() casts a ULong to a
    // scalar with no conversion for it, `i < object.size()' compares a size_t
    // with a ULong, and object[i] is non-const. A template member is only
    // checked when something uses it, and nothing in the tree packed a Vector
    // -- so a caller trying to send one over RPC was the first, and it did not
    // build.
    //
    // Both read the underlying vector directly now rather than going through
    // size() and operator[]: it is const-correct, it is one cast instead of
    // three, and it is the same walk pack_size does, which is what keeps the
    // two agreeing.
    friend binarystream& operator<<(binarystream& outstream, const Vector<T>& object)
    {
      outstream.write_std_ubyte(object.tdb());
      if (object._pointer != nullptr) {
	const std::vector<T>& elements = *(std::vector<T>*)object._pointer;
        outstream.write_std_uint((uint32_t)elements.size());
	for (size_t i = 0; i < elements.size(); i++) {
	  outstream << elements[i];
	}
      }
      return outstream;
    }

    friend binarystream& operator<<(binarystream& outstream, Vector<T>&& object)
    {
      return outstream << (const Vector<T>&)object;
    }
  
    friend binarystream& operator>>(binarystream& instream, Vector<T>& object)
    {
      uint8_t tdb = instream.read_std_ubyte();
      if ( (tdb & TDByte::IS_NULL) == TDByte::IS_NULL) {
	object.set_null();
      } else {
	// Straight at the vector, as operator<< now is. object[i] is declared
	// and never defined -- an undefined reference the moment anything
	// unpacks a Vector, which nothing did.
	if (object._pointer == nullptr) object._pointer = new std::vector<T>();
	std::vector<T>& elements = *(std::vector<T>*)object._pointer;

	uint32_t size = instream.read_std_uint();
	elements.resize((size_t)size);
	for (size_t i = 0; i < size; i++) {
	  instream >> elements[i];
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
  // CONST, because packing a Vector asks it -- and the const overload of
  // operator<< above could therefore never be instantiated. Nothing in the tree
  // had packed a const Vector, so it compiled for as long as nobody tried; a
  // caller sending one over RPC is the first, and it would not have built.
  size_t Vector<T>::std_size() const
  {
    if (this->_pointer == nullptr) throw NULL_EXCEPTION;
    return ((std::vector<T>*)this->_pointer)->size();
  }
 
  template <typename T>
  ULong Vector<T>::size() const
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

  // What operator<< above will write, exactly: the descriptor byte, then the
  // count, then each element -- so this ASKS THE ELEMENTS rather than working
  // it out from the type.
  //
  // It had two faults, and the first is the one that made a tensor over RPC
  // impossible. `this->_pointer' was cast to std::string* to ask its size(),
  // copied from String::pack_size, which reads a std::vector through a
  // std::string's layout: the answer is whatever those bytes happen to mean and
  // is not the element count. Connector sizes its frame with this, so the frame
  // was the wrong length and the far side read the next value out of the
  // remains of this one.
  //
  // The second would have been wrong even with the cast right. An element does
  // not pack as SIZEOF(T::TDB) bytes -- it packs as its own pack_size, which is
  // that plus its descriptor byte (Object::pack_size), so the old arithmetic
  // was short by one byte per element. And it cannot be a multiplication at
  // all: Vector<String> holds elements of different lengths, and no per-type
  // constant describes them. Summing what each element reports is both correct
  // and the only thing that stays correct when T is variable-length.
  template <typename T>
  int64_t Vector<T>::pack_size() const
  {
    if (this->_pointer == nullptr) return 1;

    const std::vector<T>& elements = *(std::vector<T>*)this->_pointer;

    int64_t size = 1 + TDByte::SIZEOF_DWORD;   // the descriptor, then the count
    for (size_t i = 0; i < elements.size(); i++) {
      size += elements[i].pack_size();
    }
    return size;
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
