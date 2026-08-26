
#ifndef __GLOBALS_HPP__
#define __GLOBALS_HPP__

#include "types.hpp"
#include "zexception.hpp"
#include "binarystream.hpp"
#include "textstream.hpp"
#include "resulttype.hpp"
#include "isolationlevel.hpp"
#include <string>
#include <vector>

namespace Zigurat
{
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

  // Whether what a certificate grants is enforced at all. Off, a connection is
  // still encrypted and both ends still prove themselves against the authority
  // -- holding a certificate the owner issued remains the price of admission --
  // but nothing is keyed on which certificate it is. One switch, because half
  // enforced would be worse than either.
  static bool _permissions_mode;

  static bool _default_autocommit_mode;
  static Zigurat::IsolationLevel _default_isolation_level;

  static thread_local Zigurat::binarystream* _client_stream;
  static thread_local Zigurat::textstream* _echo_stream;

  // Who is on the other end of this thread's connection, and what the
  // certificate that opened it says they may reach. Both come off the
  // handshake and neither is stored anywhere: a connection carries its own
  // authority. Unidentified means a plain connection, where there is no peer
  // to ask about and everything is allowed -- turning TLS on is what turns
  // access control on.
  static thread_local bool _identified;
  static thread_local std::string _peer_subject;
  static thread_local std::vector<std::string> _peer_permissions;

  static Zigurat::Parser* _parser;
  static Zigurat::Compiler* _compiler;

 public:
  static bool reset_mode();
  static bool trace_mode();
  static bool permissions_mode();
  static bool default_autocommit_mode();
  static Zigurat::IsolationLevel default_isolation_level();

  static Zigurat::binarystream* const client_stream();
  static Zigurat::textstream* const echo_stream();

  static Zigurat::Parser* const parser();
  static Zigurat::Compiler* const compiler();

  static void set_reset_mode(bool);
  static void set_trace_mode(bool);
  static void set_permissions_mode(bool);
  static void set_default_autocommit_mode(bool);
  static void set_default_isolation_level(Zigurat::IsolationLevel);

  static bool identified();
  static const std::string& peer_subject();
  static const std::vector<std::string>& peer_permissions();

  // A permission is a path: the schema levels, then the object name. It covers
  // an object when it is that object or a prefix of it, so DEMO covers
  // DEMO::AUTHORS and DEMO::AUTHORS covers only itself. "*" covers everything.
  // Matching ignores case, because Parsi does.
  static bool permits(const std::string&);

  // The same question, asked where the answer has to be no. Throws.
  static void require_permission(const std::string&);

  static void set_client_stream(Zigurat::binarystream*);
  static void set_echo_stream(Zigurat::textstream*);

  static void set_peer(const std::string&, const std::vector<std::string>&);
  static void clear_peer();

  static void set_parser(Zigurat::Parser*);
  static void set_compiler(Zigurat::Compiler*);
};

#endif // __GLOBALS_HPP__
