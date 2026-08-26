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

// -- verbatim from engine.cpp (checked by build.sh) --------------------
struct Memory;   // opaque here; full definition stays in the library
struct BTreeIndex {
  Memory * m ;
  const char * name ;
  const uint8_t * hash_key ;
  const uint8_t * table_key ;
  int64_t catalogue_id ;
  int64_t is_unique ;
  int64_t branching_factor ;
  int64_t min_degree ;
  int64_t max_degree ;
  int64_t root_address ;
  Pointer record_pointer ;
  int64_t levels ;
  int64_t is_dependent ;
  const uint8_t * dep_hash_key ;
};
// ----------------------------------------------------------------------

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

// -- the B-tree index tier ---------------------------------------------
// A consumer holds a BTreeIndex instance by value at file scope, fills
// its fields the way the defindex expansion's attach does -- intern_key
// on the 20-byte index key, the table's key, catalogue id, branching --
// and calls bt_select_record once per session to find or create its
// catalogue record. Its table's map/unmap virtuals then call bt_map and
// bt_unmap; the cursors judge visibility themselves and need no guard
// from the caller.
Pointer pointer_null ();
const uint8_t * intern_key (const uint8_t * key);
void bt_select_record (BTreeIndex * idx);
void bt_map (BTreeIndex * idx, int64_t k, int64_t value);
void bt_unmap (BTreeIndex * idx, int64_t k, int64_t value);
void bt_map_multi (BTreeIndex * idx, int64_t * ks, int64_t value);
void bt_unmap_multi (BTreeIndex * idx, int64_t * ks, int64_t value);
void bt_unmap_key (BTreeIndex * idx, int64_t k);
void bt_drop_storage (BTreeIndex * idx, Memory * m);
bool bt_cursor_rows_deep (BTreeIndex * idx, void * user,
                          bool (*rcb) (void * c, Pointer * p));
bool bt_cursor_equal_multi (BTreeIndex * idx, int64_t * ks, void * user,
                            bool (*rcb) (void * c, Pointer * p));
void bt_cursor_not_equal (BTreeIndex * idx, int64_t k, void * user,
                          bool (*rcb) (void * c, Pointer * p));
void bt_cursor_less_than (BTreeIndex * idx, int64_t k, void * user,
                          bool (*rcb) (void * c, Pointer * p));
void bt_cursor_less_than_equal (BTreeIndex * idx, int64_t k, void * user,
                                bool (*rcb) (void * c, Pointer * p));
void bt_cursor_greater_than (BTreeIndex * idx, int64_t k, void * user,
                             bool (*rcb) (void * c, Pointer * p));
void bt_cursor_greater_than_equal (BTreeIndex * idx, int64_t k, void * user,
                                   bool (*rcb) (void * c, Pointer * p));

#endif
