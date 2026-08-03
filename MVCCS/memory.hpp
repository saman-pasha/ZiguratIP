
#ifndef __MEMORY_HPP__
#define __MEMORY_HPP__

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
    
    class HashKeyComparer : public std::binary_function<hashkey_ptr, hashkey_ptr, bool>
    {
      public:
	bool operator()(hashkey_ptr src, hashkey_ptr dst) const;
    };

    typedef char* buffer_t;
    typedef std::unique_lock<std::mutex> lock_t;
  
  private:
    binarystream& _hexmap_io;
    binarystream& _data_io;
    binarystream* _watcher = nullptr;

    const int64_t _page_size;
    const int64_t _hbpp;      // hex bytes per page
    int64_t _page_count = 0;

    std::multimap<hashkey_ptr, int64_t, HashKeyComparer> _page_list;
    std::multimap<hashkey_ptr, Pointer, HashKeyComparer> _free_list;

    std::mutex _hexmap_access;
    std::mutex _data_access;

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
    int64_t _free_prev_count();
    int64_t _free_next_count();
    void _free(const Pointer&);
    void _free_key(hashkey_ptr);
    void _dead_pointers(hashkey_ptr, std::list<Pointer>&);
    
    // Transaction ACID
    void _dump_control(const Pointer&, const Control&);
    void _load_control(const Pointer&, Control&);
    
    void _control_insert(BaseTable*);
    void _control_select(BaseTable*, lock_t*, lock_t*);
    void _control_update(BaseTable*, BaseTable*, lock_t*, lock_t*);
    void _control_delete(BaseTable*, lock_t*, lock_t*);

    void _transaction_push(const Pointer&);
    void _write_transaction(size_t, time_t);
    
    void _commit_pointer(const Pointer&, time_t);
    void _rollback_pointer(const Pointer&);
    
    bool _check_lock(RowLock, const Pointer&, Control&, lock_t*, lock_t*);

    // Cursor
    void _cursor(hashkey_ptr, lock_t*, lock_t*, std::function<bool (const Pointer&)>);
    
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
    control.create_time = std::time(0);
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
    control.modify_time = std::time(0);
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
    control.modify_time = std::time(0);
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

    std::lock_guard<std::mutex> hexmap_lock(this->_hexmap_access);
    std::lock_guard<std::mutex> data_lock(this->_data_access);    

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

    std::unique_lock<std::mutex> hexmap_lock(this->_hexmap_access);
    std::unique_lock<std::mutex> data_lock(this->_data_access);    

    new_object.pointer = this->_allocate(T::hash_key, new_object.pack_size());
    this->_transaction_push(old_object.pointer);
    this->_transaction_push(new_object.pointer);
    this->_control_update(&old_object, &new_object, &hexmap_lock, &data_lock);

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
    
    std::unique_lock<std::mutex> hexmap_lock(this->_hexmap_access);
    std::unique_lock<std::mutex> data_lock(this->_data_access);    

    this->_transaction_push(object.pointer);
    this->_control_delete(&object, &hexmap_lock, &data_lock);

    object.unmap();
    
    this->_hexmap_io.flush();
    this->_data_io.flush();
  }

  template <typename T>
  size_t Memory::truncate()
  {
    return this->truncate(T::hash_key);
  }

  template <typename T>
  void Memory::cursor(hashkey_ptr hash_key, std::function<bool (T&)> callback)
  {
    if (this->_watcher) {
      this->dba_watch("Cursor: (" + T::name + ")");
    }
    
    std::unique_lock<std::mutex> hexmap_lock(this->_hexmap_access);
    std::unique_lock<std::mutex> data_lock(this->_data_access);    

    this->_cursor(hash_key, &hexmap_lock, &data_lock, [&] (const Pointer& pointer) -> bool {

	T object(pointer);
	
	hexmap_lock.lock();
	data_lock.lock();

	this->_data_io.seekg(this->_pointer_data_address(object.pointer), std::ios::beg);
	this->_data_io >> object;

	hexmap_lock.unlock();
	data_lock.unlock();
	
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
