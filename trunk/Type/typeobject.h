
#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "tdbyte.h"
#include "zexception.h"

namespace Zigurat
{

  class Bool;
  class String;
  class binarystream;
  class textstream;

  class Object
  {
  protected:
    static const uint8_t TDB;
    void* _pointer = nullptr;

  public:
    static const std::string NULL_STRING;
    static const ZiguratException NULL_EXCEPTION;

    virtual uint8_t tdb() const;
    virtual Bool is_null() const;
    virtual Bool IS_NULL() const;
    virtual const void* pointer() const;
    virtual int64_t pack_size() const;
    virtual std::string to_std_string() const;
    virtual String to_string() const;
    virtual String TO_STRING() const;
    virtual ~Object();

    friend binarystream& operator<<(binarystream&, Object&&);
    friend binarystream& operator<<(binarystream&, const Object&);
    friend binarystream& operator>>(binarystream&, Object&);

    friend textstream& operator<<(textstream&, Object&&);
    friend textstream& operator<<(textstream&, const Object&);
  };

}

#endif // __OBJECT_H__
