
#ifndef __CONNECTOR_HPP__
#define __CONNECTOR_HPP__

#include "resulttype.hpp"
#include <vector>
#include <string>
#include "binarystream.hpp"
#include "typevector.hpp"
#include "isolationlevel.hpp"
#include "tls.hpp"

namespace Zigurat
{

  class Bool;
  class Char;
  class Byte;
  class UByte;
  class Short;
  class UShort;
  class Int;
  class UInt;
  class Long;
  class ULong;
  class Float;
  class Double;
  class Real;
  class Timestamp;
  class String;
  class Text;

  class Connector
  {
  protected:
    binarystream* _stream = nullptr;
    std::string _host;
    std::string _port;
    size_t _transaction_id = 0;
    std::vector<std::string> _meta_columns;

  public:
    Connector();

    // Connects the way home/etc/connector.conf says to, secure or not.
    void open();

    // A plain connection, and an authenticated encrypted one. The second needs
    // this client's own certificate and the authority that must have signed the
    // server's, the same three files a server is configured with.
    void open(std::string, std::string, bool = true, int = 0);
    void open(const TLS::HandshakeParameters&, std::string, std::string, bool = true, int = 0);
    bool is_open();
    void close();
    size_t transaction_id();
    ULong TRANSACTION_ID();
    binarystream& stream();
    ResultType result();
    void function(std::string);
    std::string echo(std::string);
    void compile(std::string);
    void call(std::string);
    void auto_commit(bool);
    void isolate(IsolationLevel);
    void commit();
    void rollback();
    void fetch();
    template <typename F, typename... Ts> void fetch(F&, Ts&...);
    template <typename F, typename... Ts> void FETCH(F&, Ts&...);
    std::vector<std::string> columns();
    
    template <typename T> void write(T&&);
    void write_bool(Bool&&);
    void write_char(Char&&);
    void write_byte(Byte&&);
    void write_ubyte(UByte&&);
    void write_short(Short&&);
    void write_ushort(UShort&&);
    void write_int(Int&&);
    void write_uint(UInt&&);
    void write_long(Long&&);
    void write_ulong(ULong&&);
    void write_float(Float&&);
    void write_double(Double&&);
    void write_real(Real&&);
    void write_timestamp(Timestamp&&);
    void write_string(String&&);
    void write_text(Text&&);
    template <typename T> void write_vector(Vector<T>&&);
    
    template <typename T> void write(const T&);
    void write_bool(const Bool&);
    void write_char(const Char&);
    void write_byte(const Byte&);
    void write_ubyte(const UByte&);
    void write_short(const Short&);
    void write_ushort(const UShort&);
    void write_int(const Int&);
    void write_uint(const UInt&);
    void write_long(const Long&);
    void write_ulong(const ULong&);
    void write_float(const Float&);
    void write_double(const Double&);
    void write_real(const Real&);
    void write_timestamp(const Timestamp&);
    void write_string(const String&);
    void write_text(const Text&);
    template <typename T> void write_vector(const Vector<T>&);

    // Read Field
    template <typename T> T read();
    Bool read_bool();
    Char read_char();
    Byte read_byte();
    UByte read_ubyte();
    Short read_short();
    UShort read_ushort();
    Int read_int();
    UInt read_uint();
    Long read_long();
    ULong read_ulong();
    Float read_float();
    Double read_double();
    Real read_real();
    Timestamp read_timestamp();
    String read_string();
    Text read_text();
    template <typename T> Vector<T> read_vector();
    
    template <typename T> void read(T&);
    void read_bool(Bool&);
    void read_char(Char&);
    void read_byte(Byte&);
    void read_ubyte(UByte&);
    void read_short(Short&);
    void read_ushort(UShort&);
    void read_int(Int&);
    void read_uint(UInt&);
    void read_long(Long&);
    void read_ulong(ULong&);
    void read_float(Float&);
    void read_double(Double&);
    void read_real(Real&);
    void read_timestamp(Timestamp&);
    void read_string(String&);
    void read_text(Text&);
    template <typename T> void read_vector(Vector<T>&);
    
    // Parsi UpperCase
    template <typename T> void WRITE(T&&);
    void WRITE_BOOL(Bool&&);
    void WRITE_CHAR(Char&&);
    void WRITE_BYTE(Byte&&);
    void WRITE_UBYTE(UByte&&);
    void WRITE_SHORT(Short&&);
    void WRITE_USHORT(UShort&&);
    void WRITE_INT(Int&&);
    void WRITE_UINT(UInt&&);
    void WRITE_LONG(Long&&);
    void WRITE_ULONG(ULong&&);
    void WRITE_FLOAT(Float&&);
    void WRITE_DOUBLE(Double&&);
    void WRITE_REAL(Real&&);
    void WRITE_TIMESTAMP(Timestamp&&);
    void WRITE_STRING(String&&);
    void WRITE_TEXT(Text&&);
    template <typename T> void WRITE_VECTOR(Vector<T>&&);
    
    template <typename T> void WRITE(const T&);
    void WRITE_BOOL(const Bool&);
    void WRITE_CHAR(const Char&);
    void WRITE_BYTE(const Byte&);
    void WRITE_UBYTE(const UByte&);
    void WRITE_SHORT(const Short&);
    void WRITE_USHORT(const UShort&);
    void WRITE_INT(const Int&);
    void WRITE_UINT(const UInt&);
    void WRITE_LONG(const Long&);
    void WRITE_ULONG(const ULong&);
    void WRITE_FLOAT(const Float&);
    void WRITE_DOUBLE(const Double&);
    void WRITE_REAL(const Real&);
    void WRITE_TIMESTAMP(const Timestamp&);
    void WRITE_STRING(const String&);
    void WRITE_TEXT(const Text&);
    template <typename T> void WRITE_VECTOR(const Vector<T>&);

    // READ Field
    uint8_t READ_tdbyte();
    template <typename T> T READ();
    Bool READ_BOOL();
    Char READ_CHAR();
    Byte READ_BYTE();
    UByte READ_UBYTE();
    Short READ_SHORT();
    UShort READ_USHORT();
    Int READ_INT();
    UInt READ_UINT();
    Long READ_LONG();
    ULong READ_ULONG();
    Float READ_FLOAT();
    Double READ_DOUBLE();
    Real READ_REAL();
    Timestamp READ_TIMESTAMP();
    String READ_STRING();
    Text READ_TEXT();
    template <typename T> Vector<T> READ_VECTOR();
    
    template <typename T> void READ(T&);
    void READ_BOOL(Bool&);
    void READ_CHAR(Char&);
    void READ_BYTE(Byte&);
    void READ_UBYTE(UByte&);
    void READ_SHORT(Short&);
    void READ_USHORT(UShort&);
    void READ_INT(Int&);
    void READ_UINT(UInt&);
    void READ_LONG(Long&);
    void READ_ULONG(ULong&);
    void READ_FLOAT(Float&);
    void READ_DOUBLE(Double&);
    void READ_REAL(Real&);
    void READ_TIMESTAMP(Timestamp&);
    void READ_STRING(String&);
    void READ_TEXT(Text&);
    template <typename T> void READ_VECTOR(Vector<T>&);

    virtual ~Connector();
  };

  template <typename F, typename... Ts> 
  void Connector::fetch(F& first, Ts&... ts)
  {
    this->_stream->unpack(first, ts...);
  }
    
  template <typename F, typename... Ts> 
  void Connector::FETCH(F& first, Ts&... ts)
  {
    this->_stream->unpack(first, ts...);
  }
    
  template <typename T>
  void Connector::write(T&& value)
  {
    *this->_stream << std::forward<T&&>(value);
  }
	
  template <typename T>
  void Connector::write(const T& value)
  {
    *this->_stream << value;
  }
	
  template <typename T>
  void Connector::WRITE(T&& value)
  {
    *this->_stream << std::forward<T&&>(value);
  }
	
  template <typename T>
  void Connector::WRITE(const T& value)
  {
    *this->_stream << value;
  }
	
  template <typename T>
  T Connector::read()
  {
    T value;
    *this->_stream >> value;
    return value;
  }
	
  template <typename T>
  void Connector::read(T& value)
  {
    *this->_stream >> value;
  }

  template <typename T>
  T Connector::READ()
  {
    T value;
    *this->_stream >> value;
    return value;
  }
	
  template <typename T>
  void Connector::READ(T& value)
  {
    *this->_stream >> value;
  }

  template <typename T>
  Vector<T> Connector::read_vector()
  {
    Vector<T> aVector;
    this->_stream >> aVector;
    return aVector;
  }

  template <typename T>
  void Connector::read_vector(Vector<T>& aVector)
  {
    this->_stream >> aVector;
  }

  template <typename T>
  Vector<T> Connector::READ_VECTOR()
  {
    Vector<T> aVector;
    this->_stream >> aVector;
    return aVector;
  }

  template <typename T>
  void Connector::READ_VECTOR(Vector<T>& aVector)
  {
    this->_stream >> aVector;
  }

  template <typename T>
  void Connector::write_vector(const Vector<T>& aVector)
  {
    *this->_stream << aVector;
  }
	
  template <typename T>
  void Connector::write_vector(Vector<T>&& aVector)
  {
    *this->_stream << std::forward<Vector<T>&&>(aVector);
  }

  template <typename T>
  void Connector::WRITE_VECTOR(const Vector<T>& aVector)
  {
    *this->_stream << aVector;
  }
	
  template <typename T>
  void Connector::WRITE_VECTOR(Vector<T>&& aVector)
  {
    *this->_stream << std::forward<Vector<T>&&>(aVector);
  }

}

#endif // __CONNECTOR_HPP__
