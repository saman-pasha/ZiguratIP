
#ifndef __TEXT_HPP__
#define __TEXT_HPP__


#include <cstdint>
#include "typeobject.hpp"
#include "typeulong.hpp"
#include <string>

namespace Zigurat
{

  class String;

  class Text : public Object
  {
  public:
    typedef std::string SCT;
    static const uint8_t TDB;

    virtual uint8_t tdb() const override;

    Text();
    Text(std::nullptr_t);
    Text(std::string&&);
    Text(const std::string&);
    Text(Text&&);
    Text(const Text&);
    Text(const char*);
    Text(const char*, size_t);
    Text(String&&);
    Text(const String&);

    Text& operator=(std::nullptr_t);
    Text& operator=(std::string&&);
    Text& operator=(const std::string&);
    Text& operator=(Text&&);
    Text& operator=(const Text&);
    Text& operator=(const char*);

    virtual bool operator==(std::nullptr_t) const;
    virtual bool operator==(std::string&&) const;
    virtual bool operator==(const std::string&) const;
    virtual bool operator==(Text&&) const;
    virtual bool operator==(const Text&) const;
    
    virtual bool operator!=(std::nullptr_t) const;
    virtual bool operator!=(std::string&&) const;
    virtual bool operator!=(const std::string&) const;
    virtual bool operator!=(Text&&) const;
    virtual bool operator!=(const Text&) const;
    
    virtual bool operator<(const Text&) const;
    virtual bool operator<=(const Text&) const;
    virtual bool operator>(const Text&) const;
    virtual bool operator>=(const Text&) const;

    virtual Text operator+(const char*) const;
    friend Text operator+(const char*, Text);
    virtual Text operator+(Text) const;

    virtual void set_null();
    virtual const std::string& value() const;

    virtual size_t std_size() const;
    virtual ULong size() const;
    virtual ULong SIZE() const;

    virtual size_t std_length() const;
    virtual ULong length() const;
    virtual ULong LENGTH() const;

    virtual int64_t pack_size() const override;
    virtual std::string to_std_string() const override;

    virtual ~Text();

    friend binarystream& operator<<(binarystream&, Text&&);
    friend binarystream& operator<<(binarystream&, const Text&);
    friend binarystream& operator>>(binarystream&, Text&);
  };
	
}

#endif // __TEXT_HPP__
