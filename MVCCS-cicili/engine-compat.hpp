// engine-compat.hpp -- the spellings the Parsi compiler's generated code
// already uses, kept alive over the Cicili engine.
//
// The DML compilers emit exactly four shapes:
//
//   Globals::memory()->cursor<T>( [&](T& r) -> bool {...} );
//   T::INDEX.cursor_equal(v, [&](T& r) -> bool {...});          // + the range family
//   T::INDEX.cursor_equal(v, [&](Zigurat::BTreeIndex<T, Rest...>& d) -> bool {...});
//   Globals::memory()->online_insert(obj) / online_update(a, b) / truncate<T>()
//
// Rewriting those emitters would churn every compiler file and buy
// nothing, so this header keeps the spellings and changes the engine
// under them: Zigurat::BTreeIndex<T, Ks...> is defined HERE as a thin
// adapter over the engine's ::BTreeIndex and bt_* functions, and
// Globals:: is a namespace of inline forwards to the library's
// globals_* state. Generated code includes THIS header and never the
// old MVCCS ones; the two engines never meet in one translation unit.
//
// CAPTURING LAMBDAS CROSS THE BOUNDARY as a context pointer and a
// static trampoline -- the engine's callbacks are C-shaped on purpose,
// and the adapter is where the two calling conventions meet.
//
// STRING INDEX KEYS ride as engine_text_key's 64-bit fold -- see the
// note beside it in engine.cicili: collisions cost a wasted row visit,
// never a wrong answer, because every generated lookup re-applies its
// full WHERE predicate to each row the index hands back.

#ifndef MVCCS_CICILI_ENGINE_COMPAT_HPP
#define MVCCS_CICILI_ENGINE_COMPAT_HPP

#include "engine.hpp"
#include <string>
#include <vector>
#include <array>
#include <pthread.h>
#include "typeobject.hpp"
#include "typebool.hpp"
#include "typeubyte.hpp"
#include "typebyte.hpp"
#include "typechar.hpp"
#include "typeint.hpp"
#include "typelong.hpp"
#include "typedouble.hpp"
#include "typestring.hpp"
#include "typetext.hpp"
#include "typeshort.hpp"
#include "typeushort.hpp"
#include "typeuint.hpp"
#include "typeulong.hpp"
#include "typefloat.hpp"
#include "typereal.hpp"
#include "typetimestamp.hpp"
#include "typevector.hpp"
#include "resulttype.hpp"
#include "zexception.hpp"
#include "utility.hpp"
#include "/home/user/ZiguratIP/StreamIO/textstream.hpp"
#include <iostream>
#include <utility>

int64_t engine_text_key (const char * s, size_t n);

// THE PARSI KEYWORD MACROS, verbatim from the old globals.hpp: the
// expression compiler emits these spellings and generated code cannot
// stand without them.
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

// THE PARSI SPELLINGS. The tokenizer upper-cases every identifier, so
// generated code says LONG and STRING; the old globals.hpp carried these
// aliases and generated code cannot stand without them.
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

inline int64_t engine_key64 (const Zigurat::Text& s);   // defined after key64 family

namespace Zigurat {
  typedef uint8_t hashkey_t[20];
  typedef const uint8_t* hashkey_ptr;
}

// ---- a value becomes an int64 index key ------------------------------
// The whole integral family folds losslessly; floating keys are NOT
// offered on purpose -- a Double folded to int64 would order wrongly,
// and a schema that wants one should hear it from the compiler here.
inline int64_t engine_key64 (int64_t v)                 { return v; }
inline int64_t engine_key64 (const Zigurat::Long& v)    { return v.value(); }
inline int64_t engine_key64 (const Zigurat::Int& v)     { return (int64_t)v.value(); }
inline int64_t engine_key64 (const Zigurat::Bool& v)    { return (int64_t)(v.value() ? 1 : 0); }
inline int64_t engine_key64 (const Zigurat::Char& v)    { return (int64_t)v.value(); }
inline int64_t engine_key64 (const Zigurat::Byte& v)    { return (int64_t)v.value(); }
inline int64_t engine_key64 (const Zigurat::UByte& v)   { return (int64_t)v.value(); }
inline int64_t engine_key64 (const Zigurat::Short& v)   { return (int64_t)v.value(); }
inline int64_t engine_key64 (const Zigurat::UShort& v)  { return (int64_t)v.value(); }
inline int64_t engine_key64 (const Zigurat::UInt& v)    { return (int64_t)v.value(); }
inline int64_t engine_key64 (const Zigurat::ULong& v)   { return (int64_t)v.value(); }
inline int64_t engine_key64 (const Zigurat::Timestamp& v) { return (int64_t)v.value(); }
inline int64_t engine_key64 (const std::string& s)      { return engine_text_key(s.data(), s.size()); }
inline int64_t engine_key64 (const Zigurat::String& s)  { return engine_key64(s.value()); }
inline int64_t engine_key64 (const Zigurat::Text& s)    { return engine_key64(s.value()); }

// ---- capturing lambdas over C-shaped callbacks -----------------------
template <typename T, typename F>
struct EngineRowShim {
  Memory* m; F* f;
  static bool call (void* c, Pointer* p) {
    EngineRowShim* s = (EngineRowShim*)c;
    T row;
    row.pointer = *p;
    read_row(s->m, &row);
    return (*s->f)(row);
  }
};

// ---- the index, under its old spelling -------------------------------
// One template serves the outer static, the middle levels and the
// innermost: with more than one key type left, cursors hand DEPENDENT
// index handles; with one, they hand rows. The engine's stack-built
// dependent instances (bt_dependent) are copied into the next handle,
// exactly as the defindex expansion treats them.
namespace Zigurat {

template <typename T, typename... Ks> struct BTreeIndex;

template <typename T, typename K1, typename... Kr>
struct BTreeIndex<T, K1, Kr...> {
  ::BTreeIndex bt;
  // the generated table's lazy attach: set at static initialization
  // (which touches no store), called before any walk
  void (*ensure) () = nullptr;

  static constexpr bool INNER = (sizeof...(Kr) == 0);
  typedef BTreeIndex<T, Kr...> Dep;   // meaningful only when !INNER

  template <typename F>
  struct DepShim {
    F* f;
    static bool call (void* c, ::BTreeIndex* dep) {
      DepShim* s = (DepShim*)c;
      typename BTreeIndex<T, K1, Kr...>::Dep handle;
      handle.bt = *dep;
      return (*s->f)(handle);
    }
  };

  template <typename F>
  void cursor (F f) {
    if (ensure) ensure();
    if constexpr (INNER) {
      EngineRowShim<T, F> s{ bt.m, &f };
      bt_cursor_rows_deep(&bt, &s, &EngineRowShim<T, F>::call);
    } else {
      DepShim<F> s{ &f };
      bt_cursor_dep(&bt, &s, &DepShim<F>::call);
    }
  }

  template <typename V, typename F>
  void cursor_equal (const V& v, F f) {
    if (ensure) ensure();
    if constexpr (INNER) {
      int64_t ks[1] = { engine_key64(v) };
      EngineRowShim<T, F> s{ bt.m, &f };
      bt_cursor_equal_multi(&bt, ks, &s, &EngineRowShim<T, F>::call);
    } else {
      DepShim<F> s{ &f };
      bt_cursor_equal_dep(&bt, engine_key64(v), &s, &DepShim<F>::call);
    }
  }

  // The range family hands rows and stays single-level, as the engine's
  // own cursors do; the WHERE compiler routes anything else to a scan.
  template <typename V, typename F>
  void cursor_not_equal (const V& v, F f) {
    if (ensure) ensure();
    static_assert(INNER, "range cursors serve the innermost level only");
    EngineRowShim<T, F> s{ bt.m, &f };
    bt_cursor_not_equal(&bt, engine_key64(v), &s, &EngineRowShim<T, F>::call);
  }
  template <typename V, typename F>
  void cursor_less_than (const V& v, F f) {
    if (ensure) ensure();
    static_assert(INNER, "range cursors serve the innermost level only");
    EngineRowShim<T, F> s{ bt.m, &f };
    bt_cursor_less_than(&bt, engine_key64(v), &s, &EngineRowShim<T, F>::call);
  }
  template <typename V, typename F>
  void cursor_less_than_equal (const V& v, F f) {
    if (ensure) ensure();
    static_assert(INNER, "range cursors serve the innermost level only");
    EngineRowShim<T, F> s{ bt.m, &f };
    bt_cursor_less_than_equal(&bt, engine_key64(v), &s, &EngineRowShim<T, F>::call);
  }
  template <typename V, typename F>
  void cursor_greater_than (const V& v, F f) {
    if (ensure) ensure();
    static_assert(INNER, "range cursors serve the innermost level only");
    EngineRowShim<T, F> s{ bt.m, &f };
    bt_cursor_greater_than(&bt, engine_key64(v), &s, &EngineRowShim<T, F>::call);
  }
  template <typename V, typename F>
  void cursor_greater_than_equal (const V& v, F f) {
    if (ensure) ensure();
    static_assert(INNER, "range cursors serve the innermost level only");
    EngineRowShim<T, F> s{ bt.m, &f };
    bt_cursor_greater_than_equal(&bt, engine_key64(v), &s, &EngineRowShim<T, F>::call);
  }
};

} // namespace Zigurat

// ---- Globals, as generated code spells it ----------------------------
Zigurat::binarystream * globals_client_stream ();
Zigurat::textstream * globals_echo_stream ();
Memory * globals_memory ();

struct EngineMemoryHandle {
  Memory* m;

  template <typename T, typename F>
  void cursor (F f) {
    EngineRowShim<T, F> s{ m, &f };
    engine_cursor(m, T::hash_key, &s, &EngineRowShim<T, F>::call);
  }
  template <typename T> void online_insert (T& o) { ::online_insert(m, T::hash_key, &o); }
  template <typename T> void online_update (T& oldr, T& newr) { ::online_update(m, T::hash_key, &oldr, &newr); }
  template <typename T> void online_delete (T& o) { ::online_delete(m, &o); }
  template <typename T> size_t truncate () {
    size_t n = ::truncate_key(m, T::hash_key);
    T::truncate_indexes();
    return n;
  }

  void begin_transaction ()    { ::begin_transaction(m); }
  void commit_transaction ()   { ::commit_transaction(m); }
  void rollback_transaction () { ::rollback_transaction(m); }
};

// HIDDEN, AND IT IS LOAD-BEARING: the server's `class Globals' (the
// parser/compiler/peer bookkeeping, in the Compiler library since the
// old engine retired) carries these exact mangled names --
// _ZN7Globals6memoryEv and friends once did, client_stream still does --
// and a dlopen'd object whose inline forwards stayed default-visible
// bound to that class through the global scope: a null pointer of the
// wrong type, dead on the first insert, back when the old engine still
// defined memory(). Hidden, every object resolves these inside itself,
// where they mean this file.
namespace Globals {
  __attribute__((visibility("hidden")))
  inline EngineMemoryHandle * memory () {
    static thread_local EngineMemoryHandle h;
    h.m = ::globals_memory();
    return &h;
  }
  __attribute__((visibility("hidden")))
  inline Zigurat::binarystream * client_stream () { return ::globals_client_stream(); }
  __attribute__((visibility("hidden")))
  inline Zigurat::textstream * echo_stream ()   { return ::globals_echo_stream(); }
}

// ---- sequences -------------------------------------------------------
// -- verbatim from engine.cpp (checked by build.sh) --------------------
struct Sequence {
  Memory * m ;
  const char * name ;
  const uint8_t * hash_key ;
  int64_t from ;
  int64_t to ;
  int64_t step ;
  int64_t initialized ;
  pthread_mutex_t guard ;
};
// ----------------------------------------------------------------------
int64_t seq_current (Sequence * s);
void    seq_set_current (Sequence * s, int64_t v);
int64_t seq_next (Sequence * s);
int64_t seq_back (Sequence * s);
void    seq_reset (Sequence * s);

// ---- what an index attacher needs beyond engine.hpp ------------------
int64_t engine_key64_bytes_fold (const uint8_t * key);   // catalogue id rule

#endif
