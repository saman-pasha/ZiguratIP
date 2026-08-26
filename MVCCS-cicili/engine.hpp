// engine.hpp -- the consumer's view of the Cicili MVCCS shared library
// (libMVCCS2.so, built from engine.cicili).
//
// WHAT A CONSUMER IS: a Parsi-compiled procedure object, a server
// binding, or a test -- C++ compiled by plain g++ against this header,
// linked against the library, speaking to the ONE engine instance in
// the process. A consumer holds a Memory* it never lays out, subclasses
// BaseTable for its rows, keeps Pointer values, and calls the free
// functions below. It never sees Streams, Statement, a transaction
// record, or a thread-local: everything RAII stays inside the library,
// behind the engine_* wrappers.
//
// THE STRUCT DEFINITIONS HERE ARE COPIED VERBATIM from the emitted
// engine.cpp -- Pointer because consumers keep them by value, BaseTable
// because generated tables subclass it and the vtable must match to the
// byte. build.sh diffs these blocks against the emitted file on every
// build; a drift is a build failure, not a latent crash.

#ifndef MVCCS_CICILI_ENGINE_HPP
#define MVCCS_CICILI_ENGINE_HPP

#include <cstdint>
#include <ctime>
#include "/home/user/ZiguratIP/StreamIO/binarystream.hpp"

typedef enum RowState {
  ROW_NONE = 0,
  ROW_INSERTED = 4,
  ROW_UPDATED = 8,
  ROW_DELETED = 12
} RowState;
typedef enum RowLock {
  LOCK_NONE = 0,
  LOCK_SHARED = 1,
  LOCK_EXCLUSIVE = 2
} RowLock;
typedef enum IsolationLevel {
  READ_UNCOMMITTED = 0,
  READ_COMMITTED = 1,
  REPEATABLE_READ = 2,
  SNAPSHOT = 3,
  SERIALIZABLE = 4
} IsolationLevel;

// -- verbatim from engine.cpp (checked by build.sh) --------------------
struct Pointer {
  const uint8_t * hash_key ;
  int64_t address ;
  int64_t size ;
};
struct BaseTable {
  Pointer pointer ;
  virtual void prepare () {
  }
  virtual void map (void * m ) {
  }
  virtual void unmap (void * m ) {
  }
  virtual int64_t pack_size () {
    return 0;
  }
  virtual void pack (Zigurat::binarystream & io ) {
  }
  virtual void unpack (Zigurat::binarystream & io ) {
  }
  virtual ~BaseTable () {
  }
};
// ----------------------------------------------------------------------

struct Memory;   // opaque: engine_memory_new / engine_memory_delete

// lifetime
Memory * engine_memory_new ();
void engine_memory_delete (Memory * m);
void memory_open (Memory * m, Zigurat::binarystream * hexmap_io,
                  Zigurat::binarystream * data_io, int64_t page_size);

// transactions -- thread-scoped, exactly as the engine keeps them
void begin_transaction (Memory * m);
void commit_transaction (Memory * m);
void rollback_transaction (Memory * m);
void engine_set_isolation (IsolationLevel level);

// rows. hash_key is the table's 20-byte key, computed by the caller --
// the Parsi compiler emits the same SHA-1-derived key the C++ engine
// used, so a store carries over.
void online_insert (Memory * m, const uint8_t * hash_key, BaseTable * obj);
void online_update (Memory * m, const uint8_t * hash_key,
                    BaseTable * old_obj, BaseTable * new_obj);
void online_delete (Memory * m, BaseTable * obj);
void read_row (Memory * m, BaseTable * obj);
size_t truncate_key (Memory * m, const uint8_t * hash_key);

// the guarded cursor: visibility judged inside, streams handled inside,
// the callback handed each visible row's pointer
void engine_cursor (Memory * m, const uint8_t * hash_key, void * ctx,
                    bool (*cb) (void *, Pointer *));

// the last inserted version, from the first of a row's history
int64_t row_latest (Memory * m, Pointer * p);

#endif
