
#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__


#include <cstdint>
#include "memorybase.hpp"
#include "pointer.hpp"
#include "rowstate.hpp"
#include "rowlock.hpp"
#include "isolationlevel.hpp"
#include "binarystream.hpp"
#include "typelong.hpp"
#include "transaction.hpp"
#include "control.hpp"
#include <string>
#include <cstring>
#include <map>
#include <list>
#include <mutex>
#include <unordered_set>
#include <pthread.h>
#include <atomic>
#include <memory>
#include <iostream>
#include <functional>

namespace Zigurat
{

  class BaseTable;
  template <typename T> class BaseSequence;
  template <typename ..._Args> class BTreeIndex;
  
  class Memory
  {
  public:
    // Memory
    static const int64_t CHUNK_SIZE;

    static const int64_t HASHKEY_COUNT;
    static const int64_t HASHKEY_SIZE;
    static const int64_t HASHKEY_LITERAL_SIZE;
    static const int64_t HASHKEY_ACTUAL_SIZE;
  
    static const int64_t PAGEFILE_CONTROL_COUNT;
    static const int64_t PAGEFILE_CONTROL_SIZE;

    static const int64_t CONTROL_COUNT;
    static const int64_t CONTROL_SIZE;

    // Hexmap
    static const uint8_t FREE_CHUNK;
    static const uint8_t FREE_STANDALONE_CHUNK;
    static const uint8_t DATA_CHUNK;
    static const uint8_t DATA_STANDALONE_CHUNK;
    static const uint8_t CONTROL_CHUNK;
    static const uint8_t CONTROL_STANDALONE_CHUNK;

    // SHA1 Checksums
    static  hashkey_t FREE_HASHKEY;
    static  hashkey_t INDICES_HASHKEY;
    static  hashkey_t INSTANCE_HASHKEY;
    static  hashkey_t TRANSACTIONS_HASHKEY;
  
    static const uint8_t MAX_CURSOR_COUNT;

    // How long a row lock is waited on before the transaction gives up, in
    // milliseconds. Two sessions can each hold a lock the other is waiting for,
    // and without a bound the whole server sits there forever. 0 waits forever.
    static int lock_wait_timeout_ms;

    // ---- the live-transaction registry -------------------------------------
    //
    // Which transaction ids are between begin and commit-or-rollback right
    // now, process-wide. A lock whose owner is not in here belongs to a
    // transaction nothing will ever finish, and _check_lock breaks it in
    // place instead of waiting on a corpse until a restart's recovery would
    // have swept it.
    //
    // What makes an id dead: commit and rollback retire it after every lock
    // is cleared; a thread that dies retires it in ~Transaction after the
    // best-effort rollback; and a process that dies takes its registry with
    // it, so after the restart's recovery anything it left stamped reads as
    // breakable. A begin on a thread whose transaction is still open
    // CONTINUES it -- id and registration stand, see begin_transaction --
    // so an open transaction is never orphaned by beginning again.
    static bool transaction_live(size_t transaction_id);
    static void transaction_retire(size_t transaction_id);
    
    class HashKeyComparer : public std::binary_function<hashkey_ptr, hashkey_ptr, bool>
    {
      public:
	bool operator()(hashkey_ptr src, hashkey_ptr dst) const;
    };

    typedef char* buffer_t;
    typedef std::unique_lock<std::mutex> lock_t;

    // ---- the version clock -------------------------------------------------
    //
    // Every version of a row carries the time it was committed (create_time) and
    // the time it was superseded or deleted (modify_time, zero while it is the
    // current one), and a reader decides what it may see by comparing those with
    // a time of its own. That comparison is only as good as the clock, and the
    // clock was std::time(0): ONE SECOND. Two versions of a row born and retired
    // inside the same second are indistinguishable, which under any load at all
    // is all of them.
    //
    // This is microseconds, and strictly increasing within the process, so that
    // no two commits ever share a stamp and `before' and `after' always mean
    // something. It is stored in the same time_t fields -- 8 bytes, unchanged --
    // so the on-disk control block is the shape it always was. A store written
    // by an older build has second-scale stamps in it, which read as "committed
    // long ago" and "retired long ago": both the right answer.
    static time_t version_time();

    // ---- the statement, for readers that span time -------------------------
    //
    // A scan visits addresses in order and takes a while about it, while a
    // writer puts a replacement row wherever the allocator has room. If the two
    // are allowed to interleave freely, a scan can pass a replacement before it
    // is committed and then find the original retired -- the row missing -- or
    // see the original and then reach the committed replacement, and count the
    // same row twice. Neither is a torn read; both are wrong.
    //
    // The answer is the ordinary one: a reader decides every row against a
    // single point in time, taken once when the statement starts. This marks
    // where a statement begins, and nests -- an inner cursor belongs to the
    // statement that is already running rather than starting one of its own.
    class Statement
    {
    private:
      bool _mine;
    public:
      explicit Statement(Memory*);
      ~Statement();

      Statement(const Statement&) = delete;
      Statement& operator=(const Statement&) = delete;

      // Did this object start the statement, or was one already running?
      bool mine() const;
    };

    // ---- serialising the two shared streams --------------------------------
    //
    // _hexmap_io and _data_io are ONE PAIR, shared by every connection. Each
    // connection gets its own thread and its own transaction --
    // Memory::transaction is static thread_local -- but not its own streams, so
    // everything that seeks or reads them has to hold both mutexes. A seek and
    // the read after it are one operation: let another thread in between and
    // the read comes back from that thread's file position instead. Those bytes
    // are then taken for a chunk header, a size, an address. What surfaces is
    // `hexmap ends inside the chunk at NNNNN' with an impossible address, and
    // further in, a walk through a Pointer built out of somebody else's file
    // offset.
    //
    // WHY THIS IS A CLASS AND NOT TWO lock_guards AT EACH ENTRY POINT. Two
    // things stop that working, and both are real paths through this engine:
    //
    //   NESTING. online_insert holds both mutexes and then calls object.map(),
    //   which is the row's indexes -- and a BTreeIndex has to take them too,
    //   because procedure code reaches it directly through cursor_equal with
    //   nothing held. std::mutex is not recursive, so an index that locked
    //   unconditionally would deadlock the write path, and one that never
    //   locked would leave the read path unguarded. It was the second of those.
    //
    //   HANDING THEM BACK. A scan cannot hold these across a caller's callback:
    //   the callback is procedure code, and it updates the row it was handed.
    //   _cursor and BTreeIndex::_cursor_output therefore release them around
    //   it, and _check_lock releases them while it waits on somebody else's row
    //   lock. Anything reached during those windows has to take them itself.
    //
    // So a guard is the real holder only when this thread is not already
    // holding them, and `held()' answers that honestly at every moment -- it is
    // null exactly while this thread holds nothing. A nested guard is then a
    // no-op, and a guard built inside a released window really does lock.
    class Streams
    {
    private:
      Memory* _memory;
      bool    _mine;
      bool    _locked;
      // Held SHARED: this guard took the read side of the lock, and every
      // read this thread makes while it stands goes through the thread's own
      // private streams -- see _hex_in and _data_in.
      bool    _shared;

      // The guard holding the streams EXCLUSIVELY on this thread, or null.
      // Published on lock and withdrawn on unlock, so it never claims a hold
      // that has already been handed back. A shared hold is tracked in
      // _tl_read_mode instead, because a shared guard owns no exclusivity to
      // pass down.
      static thread_local Streams* _held;

    public:
      explicit Streams(Memory*);
      ~Streams();

      Streams(const Streams&) = delete;
      Streams& operator=(const Streams&) = delete;

      // Did this guard take them, or was this thread already holding them?
      bool mine() const;

      // Both are safe to call twice: locking what is locked, or releasing what
      // has been released, does nothing.
      void lock();
      void unlock();

      // What this thread holds, for code that must hand it back around a
      // callback and cannot see the guard that took it.
      static Streams* held();
    };

    // ---- the shared read side ----------------------------------------------
    //
    // Handing the engine the two store file paths is what turns it on: a
    // cursor whose isolation writes nothing (READ UNCOMMITTED, READ
    // COMMITTED, SNAPSHOT) then takes the streams guard SHARED and reads
    // through the thread's own private streams, concurrently with every
    // other such reader. REPEATABLE READ and SERIALIZABLE cursors -- which
    // stamp shared row locks as they scan -- and every writer take it
    // exclusive, exactly as before: the lock mode follows the level. Ported
    // from the Cicili engine, where it ran the twelve-worker choreography
    // three times faster; empty paths mean every guard is exclusive, the
    // shape this engine always had.
    void reader_paths(const std::string& hexmap_path, const std::string& data_path);

  private:
    pthread_rwlock_t _streams_rw;
    std::string _rd_hex_path;
    std::string _rd_data_path;

    // Nonzero while this thread holds the guard SHARED.
    static thread_local int _tl_read_mode;
    // The thread's private read-only streams, opened lazily from the reader
    // paths and kept for the thread's life -- WITH the store they belong to:
    // the epoch stamps which reader_paths() call opened them, so a thread
    // whose streams were opened on an earlier store (the test suite hops
    // between stores; the server never does) reopens on the current one
    // instead of reading a file that is gone.
    static std::atomic<uint64_t> _reader_epoch;
    uint64_t _reader_stamp = 0;
    static thread_local uint64_t _tl_rd_epoch;
    static thread_local binarystream* _tl_rd_hex;
    static thread_local binarystream* _tl_rd_data;
    // The next Streams constructed on this thread would LIKE to be shared;
    // the constructor consumes the flag and grants it only when it is safe.
    static thread_local int _tl_want_shared;

    bool _reader_eligible();
    void _reader_ensure();
    // Which stream a READ goes through: the thread's private one under a
    // shared guard, the canonical one everywhere else -- above all under a
    // write guard, where the canonical stream is the only place this
    // thread's own unflushed bytes can be read back from.
    binarystream& _hex_in();
    binarystream& _data_in();

    binarystream& _hexmap_io;
    binarystream& _data_io;
    binarystream* _watcher = nullptr;

    const int64_t _page_size;
    const int64_t _hbpp;      // hex bytes per page
    int64_t _page_count = 0;

    std::multimap<hashkey_ptr, int64_t, HashKeyComparer> _page_list;
    std::multimap<hashkey_ptr, Pointer, HashKeyComparer> _free_list;


    std::mutex _page_list_access;
    std::mutex _free_list_access;

    bool _initialized = false;
    void _initialize();
    
    // Allocation
    void _full_hexmap(int64_t, int64_t);
    void _full_hexmap(const Pointer&);
    void _free_hexmap(int64_t, int64_t);
    
    void _allocate_page(hashkey_ptr, int64_t);
    int64_t _allocate_new_page(hashkey_ptr);    
    Pointer _allocate(hashkey_ptr, int64_t);

    // Allocation
    Pointer _pointer(hashkey_ptr, const int64_t&);
    Pointer _pointer(hashkey_ptr, const Long&);
    Pointer _pointer(hashkey_ptr, int64_t, bool&);
    Pointer _pointer(hashkey_ptr, int64_t, bool&, RowState&, RowLock&);

    void _free_list_insert(hashkey_ptr, const Pointer&);
    int64_t _free_run_count(int64_t, int64_t);
    int64_t _free_prev_count();
    int64_t _free_next_count();
    void _free(const Pointer&);
    void _free_key(hashkey_ptr);
    void _dead_pointers(hashkey_ptr, std::list<Pointer>&);
    
    // Transaction ACID
    void _dump_control(const Pointer&, const Control&);
    void _load_control(const Pointer&, Control&);
    
    void _control_insert(BaseTable*);
    void _control_select(BaseTable*, Streams*);
    void _control_update(BaseTable*, BaseTable*, Streams*);
    void _control_delete(BaseTable*, Streams*);

    void _transaction_push(const Pointer&);
    void _write_transaction(size_t, time_t);
    
    void _commit_pointer(const Pointer&, time_t);
    // as_recovery bypasses the ownership guard: recovery -- at startup or
    // lazily from _check_lock on a dead transaction's lock -- rolls back
    // exactly the foreign stages the guard exists to protect.
    void _rollback_pointer(const Pointer&, bool as_recovery = false);
    static std::mutex _live_txn_mutex;
    static std::unordered_set<size_t> _live_txn_ids;
    static void _transaction_register(size_t fresh_id);
    
    bool _check_lock(RowLock, const Pointer&, Control&, Streams*);

    // Whether a row is visible at READ COMMITTED. Answers without waiting for
    // anything, which is the whole point of it.
    //
    // WHY IT IS NOT _check_lock. _check_lock is a WRITER's question -- "may I
    // have this row?" -- and the only answer to that is to wait until whoever
    // holds it is finished. A reader at READ COMMITTED is asking something else:
    // "what was committed here?", and the answer is sitting at the address it is
    // already looking at, because an update writes the new version somewhere
    // else and leaves this one alone until it commits.
    //
    // Reading through _check_lock made a reader wait for a writer it had no
    // business waiting for, and that wait is what lost the row: by the time it
    // ended the old version had been retired and the new one was at an address
    // the scan had already passed. A transaction that had done nothing but read
    // saw neither version of a row that existed throughout.
    bool _read_committed(const Pointer&, Control&);

    // Was this version of the row the current one at time AT? Committed by then,
    // and not yet superseded or deleted by then. Exactly one version of a row
    // satisfies it for any given time: an update stamps the old version's
    // modify_time and the new one's create_time with the SAME commit time, so
    // the two conditions change over at the same instant and never overlap.
    bool _alive_at(const Control&, time_t);

    // The point in time this read is against: the statement's, when one is
    // running, and otherwise now.
    time_t _snapshot_time();

    // Flush both files and then push them to the disk, data before hexmap.
    // Every durability point in the store goes through this.
    void _sync();

    // Whether one record is visible to the transaction now running, by the same
    // rules _cursor applies to each record it walks past.
    //
    // _cursor is where those rules live, and everything that reads the store
    // went through it -- including an index looking up the values of a key,
    // which is how a rolled back insert stayed out of the index without anyone
    // having to unmap it. Values are a chain now rather than a hash keyed
    // bucket, so the walk is the index's own and it has to ask the question
    // itself.
    //
    // Under SNAPSHOT the answer can be an *older* version of the record, so the
    // pointer is taken by reference and moved to whichever version this
    // transaction is entitled to see, exactly as _cursor moves it.
    //
    // The one thing it does not do is _cursor's retry: finding a row that
    // changed under a repeatable read makes _cursor roll back to the start of
    // the query and walk the whole hash key again, which is a scan's answer to
    // the problem and means nothing when a single record was asked about.
    bool _visible(Pointer&, Streams*);

    // Cursor
    void _cursor(hashkey_ptr, Streams*, std::function<bool (const Pointer&)>);
    
    // Pointer Helper Functions
    int64_t _pointer_data_count(int64_t);
    int64_t _pointer_actual_count(int64_t);
    int64_t _pointer_round_size(int64_t);
    int64_t _pointer_actual_size(int64_t);
    int64_t _pointer_hexmap_address(int64_t);
    int64_t _pointer_hexmap_data_address(int64_t);
    int64_t _pointer_data_address(int64_t);

    int64_t _pointer_data_count(const Pointer&);
    int64_t _pointer_actual_count(const Pointer&);
    int64_t _pointer_round_size(const Pointer&);
    int64_t _pointer_actual_size(const Pointer&);
    int64_t _pointer_hexmap_address(const Pointer&);
    int64_t _pointer_hexmap_data_address(const Pointer&);
    int64_t _pointer_data_address(const Pointer&);
    
    // ISUD Data
    template <typename T> void _offline_insert(hashkey_ptr, T&);
    template <typename T> void _offline_insert(T&);
    template <typename T> void _offline_select(T&);
    template <typename T> void _offline_update(T&);
    template <typename T> void _offline_update(T&, T&);
    template <typename T> void _offline_delete(T&);

  public:
    Memory() = delete;
    Memory(Memory&&) = delete;
    Memory(const Memory&) = delete;
    Memory(binarystream&, binarystream&, int64_t);

    // Cursor
    template <typename T> void cursor(hashkey_ptr, std::function<bool (T&)>);
    template <typename T> void cursor(std::function<bool (T&)>);
    
    // Transaction Atomicity
    static thread_local Transaction transaction;
    
    void begin_transaction();
    void commit_transaction();
    void commit_transaction_until(time_t);
    void rollback_transaction();
    void rollback_transaction_to(time_t);
    size_t transaction_id();

    // ISUD Data
    template <typename T> void online_insert(T&);
    template <typename T> void online_update(T&, T&);
    template <typename T> void online_delete(T&);

    // Reclaims the space of every settled deleted row of one table and hands
    // whole freed pages back to the allocator. Returns how many rows went.
    size_t truncate(hashkey_ptr);
    template <typename T> size_t truncate();

    // Database Administration helpers
    void dba_pagefiles(binarystream&);
    void dba_pointers(binarystream&);
    void dba_attach_watcher(binarystream*);
    void dba_detach_watcher();
    void dba_watch(std::string&&);

    virtual ~Memory();

    template <typename> friend class Zigurat::BaseSequence;
    template <typename...> friend class Zigurat::BTreeIndex;
  };
  
  template <typename T>
  void Memory::_offline_insert(hashkey_ptr hash_key, T& object)
  {
    if (this->_watcher) {
      this->dba_watch("Offline Insert: (" + T::name + ")");
    }
    
    object.pointer = this->_allocate(hash_key, object.pack_size());

    Control control;
    control.offline_state = RowState::INSERTED;
    control.create_time = Memory::version_time();
    this->_dump_control(object.pointer, control);

    this->_full_hexmap(object.pointer);
    this->_data_io.seekp(this->_pointer_data_address(object.pointer), std::ios::beg);
    this->_data_io << object;

    this->_hexmap_io.flush();
    this->_data_io.flush();
  }
  
  template <typename T>
  void Memory::_offline_insert(T& object)
  {
    this->_offline_insert(T::hash_key, object);
  }

  template <typename T>
  void Memory::_offline_select(T& object)
  {
    this->_data_io.seekg(this->_pointer_data_address(object.pointer), std::ios::beg);
    this->_data_io >> object;
  }

  template <typename T>
  void Memory::_offline_update(T& object)
  {
    if (this->_watcher) {
      this->dba_watch("Offline Update: (" + T::name + ", " + std::to_string(object.pointer.address) + ")");
    }
    
    Control control;
    this->_load_control(object.pointer, control);
    control.modify_time = Memory::version_time();
    this->_dump_control(object.pointer, control);

    this->_data_io.seekp(this->_pointer_data_address(object.pointer), std::ios::beg);
    this->_data_io << object;

    this->_hexmap_io.flush();
    this->_data_io.flush();
  }

  template <typename T>
  void Memory::_offline_update(T& old_object, T& new_object)
  {
    if (this->_watcher) {
      this->dba_watch("Offline Update: (" + T::name + ", " + std::to_string(old_object.pointer.address) + ")");
    }
    
    new_object.pointer = old_object.pointer;

    Control control;
    this->_load_control(old_object.pointer, control);
    control.modify_time = Memory::version_time();
    this->_dump_control(new_object.pointer, control);

    this->_data_io.seekp(this->_pointer_data_address(new_object.pointer), std::ios::beg);
    this->_data_io << new_object;

    this->_hexmap_io.flush();
    this->_data_io.flush();
  }

  template <typename T>
  void Memory::_offline_delete(T& object)
  {
    if (this->_watcher) {
      this->dba_watch("Offline Delete: (" + T::name + ", " + std::to_string(object.pointer.address) + ")");
    }
    
    this->_free(object.pointer);
  }

  template <typename T>
  void Memory::online_insert(T& object)
  {
    if (this->_watcher) {
      this->dba_watch("Online Insert: (hash_key: " + T::name + ")");
    }
    
    object.prepare();

    // object.map() below is this row's indexes, and a BTreeIndex takes these
    // itself when procedure code reaches it directly. Its guard nests inside
    // this one and does nothing, which is the whole reason Streams exists.
    Streams streams(this);

    object.pointer = this->_allocate(T::hash_key, object.pack_size());
    this->_transaction_push(object.pointer);
    this->_control_insert(&object);
    
    this->_full_hexmap(object.pointer);
    this->_data_io.seekp(this->_pointer_data_address(object.pointer), std::ios::beg);
    this->_data_io << object;

    object.map();

    this->_hexmap_io.flush();
    this->_data_io.flush();
  }
  
  template <typename T>
  void Memory::online_update(T& old_object, T& new_object)
  {
    if (this->_watcher) {
      this->dba_watch("Online Update: (hash_key: " + T::name + ", " + std::to_string(old_object.pointer.address) + ")");
    }
    
    new_object.prepare();

    Streams streams(this);

    new_object.pointer = this->_allocate(T::hash_key, new_object.pack_size());
    this->_transaction_push(old_object.pointer);
    this->_transaction_push(new_object.pointer);
    this->_control_update(&old_object, &new_object, &streams);

    this->_full_hexmap(new_object.pointer);
    this->_data_io.seekp(this->_pointer_data_address(new_object.pointer), std::ios::beg);
    this->_data_io << new_object;

    old_object.unmap();
    new_object.map();
    
    this->_hexmap_io.flush();
    this->_data_io.flush();
  }

  template <typename T>
  void Memory::online_delete(T& object)
  {
    if (this->_watcher) {
      this->dba_watch("Online Delete: (hash_key: " + T::name + ", " + std::to_string(object.pointer.address) + ")");
    }
    
    Streams streams(this);

    this->_transaction_push(object.pointer);
    this->_control_delete(&object, &streams);

    object.unmap();
    
    this->_hexmap_io.flush();
    this->_data_io.flush();
  }

  template <typename T>
  size_t Memory::truncate()
  {
    size_t rows = this->truncate(T::hash_key);

    // And the indexes over it, or the space comes back for the rows and stays
    // taken by the entries that pointed at them. Deleting a row already unmaps
    // it, so those entries are settled dead by the time anyone truncates.
    T::truncate_indexes();

    return rows;
  }

  template <typename T>
  void Memory::cursor(hashkey_ptr hash_key, std::function<bool (T&)> callback)
  {
    if (this->_watcher) {
      this->dba_watch("Cursor: (" + T::name + ")");
    }

    // Shared when the isolation writes nothing; the constructor decides.
    Memory::_tl_want_shared = 1;
    Streams streams(this);

    // _cursor hands the streams back before it calls this, so reading the row
    // out takes them again -- and gives them back before the caller's callback,
    // which is free to update the very row it is being handed.
    this->_cursor(hash_key, &streams, [&] (const Pointer& pointer) -> bool {

	T object(pointer);

	streams.lock();

	binarystream& din = this->_data_in();
	if (!din.good()) din.clear();
	din.seekg(this->_pointer_data_address(object.pointer), std::ios::beg);
	din >> object;

	streams.unlock();

	return callback(object);
      });
  }
  
  template <typename T>
  void Memory::cursor(std::function<bool (T&)> callback)
  {
    this->cursor(T::hash_key, callback);
  }  

}

#endif // __MEMORY_HPP__
