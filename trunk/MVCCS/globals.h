
#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include "types.h"
#include "zexception.h"
#include "binarystream.h"
#include "textstream.h"
#include "resulttype.h"
#include "isolationlevel.h"

namespace Zigurat
{
  class Memory;
  class Parser;
  class Compiler;
}

#undef THIS
#undef CAST
#undef AUTO
#undef VOID
#undef TRUE
#undef FALSE
#undef NULL

#define THIS (*this)
#define CAST static_cast
#define AUTO auto
#define VOID void
#define TRUE true
#define FALSE false
#define NULL nullptr
#define NEW(T) new T
#define TYPEOF(V) decltype(V)
#define CONTENTOF(V) (*V)
#define ADDRESSOF(V) &V
#define INCREMENT(I) I++
#define DECREMENT(I) I--
#define TYPEDESC Zigurat::TDByte
#define EXCEPTION Zigurat::ZiguratException
#define STDEXCEPTION std::exception
#define TEMPLATE_METHOD(M) template M

template <typename _First>
void DEBUG(_First&& first)
{
  std::cout << std::forward<_First>(first) << std::endl;
}

template <typename _First, typename... _Rest>
void DEBUG(_First&& first, _Rest&&... rest)
{
  std::cout << std::forward<_First>(first);
  DEBUG(std::forward<_Rest>(rest)...);
}

typedef Zigurat::Object Object;
typedef Zigurat::Bool BOOL;
typedef Zigurat::Char CHAR;
typedef Zigurat::Byte BYTE;
typedef Zigurat::UByte UBYTE;
typedef Zigurat::Short SHORT;
typedef Zigurat::UShort USHORT;
typedef Zigurat::Int INT;
typedef Zigurat::UInt UINT;
typedef Zigurat::Long LONG;
typedef Zigurat::ULong ULONG;
typedef Zigurat::Float FLOAT;
typedef Zigurat::Double DOUBLE;
typedef Zigurat::Real REAL;
typedef Zigurat::Timestamp TIMESTAMP;
typedef Zigurat::String STRING;
typedef Zigurat::Text TEXT;
template <typename T> using VECTOR = Zigurat::Vector<T>;

class Globals
{
 private:
  static bool _reset_mode;
  static bool _trace_mode;
  static bool _default_autocommit_mode;
  static Zigurat::IsolationLevel _default_isolation_level;

  static thread_local Zigurat::binarystream* _client_stream;
  static thread_local Zigurat::textstream* _echo_stream;

  static Zigurat::binarystream* _memory_hexmap_stream;
  static Zigurat::binarystream* _memory_data_stream;
  static Zigurat::Memory* _memory;
  static Zigurat::Parser* _parser;
  static Zigurat::Compiler* _compiler;

 public:
  static bool reset_mode();
  static bool trace_mode();
  static bool default_autocommit_mode();
  static Zigurat::IsolationLevel default_isolation_level();

  static Zigurat::binarystream* const client_stream();
  static Zigurat::textstream* const echo_stream();

  static Zigurat::binarystream* const memory_hexmap_stream();
  static Zigurat::binarystream* const memory_data_stream();
  static Zigurat::Memory* const memory();
  static Zigurat::Parser* const parser();
  static Zigurat::Compiler* const compiler();

  static void set_reset_mode(bool);
  static void set_trace_mode(bool);
  static void set_default_autocommit_mode(bool);
  static void set_default_isolation_level(Zigurat::IsolationLevel);

  static void set_client_stream(Zigurat::binarystream*);
  static void set_echo_stream(Zigurat::textstream*);

  static void set_memory_hexmap_stream(Zigurat::binarystream*);
  static void set_memory_data_stream(Zigurat::binarystream*);
  static void set_memory(Zigurat::Memory*);
  static void set_parser(Zigurat::Parser*);
  static void set_compiler(Zigurat::Compiler*);
};

#endif // __GLOBALS_H__
