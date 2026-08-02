#include "memory.h"
#include "memoryexception.h"
#include <cmath>
#include <thread>
#include <chrono>
#include <iterator>
#include "shahelper.h"
#include "control.h"
#include "basetable.h"
#include "transaction.h"
#include "utility.h"
#include "tcpstream.h"


namespace Zigurat
{
  // Memory
  const int64_t Memory::CHUNK_SIZE               = 16;

  const int64_t Memory::HASHKEY_SIZE             = 20;
  
  const int64_t Memory::PAGEFILE_CONTROL_COUNT   = 3;
  const int64_t Memory::PAGEFILE_CONTROL_SIZE    = 48;

  const int64_t Memory::CONTROL_COUNT            = 3;
  const int64_t Memory::CONTROL_SIZE             = CHUNK_SIZE * CONTROL_COUNT;

  // Hexmap
  const uint8_t Memory::FREE_CHUNK               = 0;   // 0,0,0,0
  const uint8_t Memory::FREE_STANDALONE_CHUNK    = 64;  // 0,64,0,0
  const uint8_t Memory::DATA_CHUNK               = 144; // 128,0,0,16
  const uint8_t Memory::DATA_STANDALONE_CHUNK    = 192; // 128,64,0,0
  const uint8_t Memory::CONTROL_CHUNK            = 176; // 128,0,32,16
  const uint8_t Memory::CONTROL_STANDALONE_CHUNK = 240; // 128,64,32,16

  // SHA1 Checksums
  hashkey_t Memory::FREE_HASHKEY         = {0xb4, 0xf4, 0x92, 0xe0, 0x09, 0x18, 0xd5, 0xa0, 0x2f, 0x11, 
					    0xfa, 0xb6, 0x07, 0x43, 0xb3, 0x88, 0x18, 0xfb, 0x14, 0x98}; // __FREE__
  hashkey_t Memory::INDICES_HASHKEY      = {0x41, 0x13, 0x40, 0x61, 0xd8, 0xb3, 0x9c, 0xc8, 0x48, 0x2b, 
					    0x4e, 0x17, 0x4b, 0xe3, 0xbe, 0x6d, 0x6a, 0x4f, 0x2c, 0xb4}; // __INDICES__
  hashkey_t Memory::INSTANCE_HASHKEY     = {0xb0, 0xe3, 0x88, 0x8b, 0x5a, 0x06, 0xdf, 0x7b, 0xf6, 0x5a, 
					    0x7e, 0x02, 0xb2, 0x06, 0x8d, 0x70, 0x42, 0xa3, 0x88, 0x1c}; // __INSTANCE__
  hashkey_t Memory::TRANSACTIONS_HASHKEY = {0xf9, 0x29, 0x10, 0x55, 0x72, 0x6a, 0x56, 0x79, 0x32, 0x98, 
					    0xad, 0x9f, 0x74, 0xd4, 0xa7, 0xac, 0x64, 0xed, 0x3e, 0x0e}; // __TRANSACTIONS__
  
  const uint8_t Memory::MAX_CURSOR_COUNT = 16;
  int Memory::lock_wait_timeout_ms = 10000;

  bool Memory::HashKeyComparer::operator()(hashkey_ptr src, hashkey_ptr dst) const
  {
    return (std::memcmp(src, dst, HASHKEY_SIZE) < 0);
  }

  thread_local Transaction Memory::transaction;  
  
  Memory::Memory(binarystream& hexmap_io, binarystream& data_io, int64_t page_size)
    : _hexmap_io(hexmap_io), _data_io(data_io), _page_size(page_size), _hbpp(page_size / CHUNK_SIZE)
  {
    this->_data_io.seekg(0, std::ios::end);
    this->_page_count = std::floor((double)this->_data_io.tellg() / this->_page_size);
    this->_data_io.clear();
    
    this->_initialize();

    this->begin_transaction();
  }

  void Memory::_initialize()
  {
    int64_t begin_address, end_address;

    for (int64_t i = 0; i < this->_page_count; i++) {

      begin_address = i * this->_page_size;
      end_address = begin_address + this->_page_size;
      
      hashkey_ptr hash_key = new hashkey_t;
      
      this->_data_io.seekg(begin_address, std::ios::beg);
      this->_data_io.read((char*)hash_key, HASHKEY_SIZE);
      
      this->_page_list.insert({hash_key, begin_address});
    }
	
    size_t length = sizeof(size_t) + sizeof(time_t);
    char buffer[length];
    size_t* transaction_id_ptr = nullptr;
    time_t* commit_time_ptr = nullptr;    
    std::map<size_t, time_t> transactions;
    
    this->_cursor(TRANSACTIONS_HASHKEY, nullptr, nullptr, [&] (const Pointer pointer) -> bool {

	this->_data_io.seekg(this->_pointer_data_address(pointer), std::ios::beg);
	this->_data_io.read(buffer, length);
      
	transaction_id_ptr = reinterpret_cast<size_t*>(buffer);
	commit_time_ptr = reinterpret_cast<time_t*>(buffer + sizeof(size_t));

	transactions[*transaction_id_ptr] = *commit_time_ptr;

	return true;
      });

    if (transactions.size() > 0) {
      std::cout << "memory restore begins" << std::endl;
    }

    bool is_data;
    RowState online_state = RowState::NONE;
    RowLock online_lock = RowLock::NONE;
    Control control;

    int64_t i = 1;
    for (auto iter = this->_page_list.begin(); iter != this->_page_list.end(); iter++, i++) {

      begin_address = iter->second + PAGEFILE_CONTROL_SIZE;
      end_address = iter->second + this->_page_size;
      
      while (begin_address < end_address) {

	Pointer pointer = this->_pointer(iter->first, begin_address, is_data, online_state, online_lock);

	if (is_data) {
	  if (online_lock != RowLock::NONE) {

	    std::cout << "uncommitted_pointer: " << iter->first << "," << begin_address << std::endl;
    
	    this->_load_control(pointer, control);

	    std::map<size_t, time_t>::iterator iter = transactions.end();
	    if ((iter = transactions.find(control.transaction_id)) != transactions.end() && iter->second > (time_t)0) {
	      this->_commit_pointer(pointer, iter->second);
	    } else {
	      this->_rollback_pointer(pointer);
	    }
	  }	  
	} else {
	  this->_free_list_insert(iter->first, pointer);
	  break;
	}
	
	begin_address += this->_pointer_actual_size(pointer);
      }

      std::cout << "memory loading... " << (100 * i) / this->_page_count << "%\r";
      std::cout.flush();
    }

    std::cout << "memory loading... 100%" << std::endl;
    
    if (transactions.size() > 0) {
      this->_cursor(TRANSACTIONS_HASHKEY, nullptr, nullptr, [&] (const Pointer& pointer) -> bool {
	  this->_free(pointer);
	  return true;
	});
      std::cout << "memory restore ends" << std::endl;
    }

    this->_initialized = true;
  }

  void Memory::_dump_control(const Pointer& pointer, const Control& control)
  {
    uint8_t online_hexmap = CONTROL_CHUNK + (uint8_t)control.online_state + (uint8_t)control.online_lock;
    uint8_t offline_hexmap = CONTROL_CHUNK + (uint8_t)control.offline_state + (uint8_t)control.offline_lock;
    
    this->_hexmap_io.seekp(this->_pointer_hexmap_address(pointer), std::ios::beg);

    this->_hexmap_io.write_std_ubyte(online_hexmap);
    this->_hexmap_io.write_std_ubyte(offline_hexmap);
    this->_hexmap_io.write_std_ubyte(CONTROL_STANDALONE_CHUNK);

    this->_data_io.seekp(pointer.address, std::ios::beg);

    this->_data_io.write_std_time(control.online_time);  
    this->_data_io.write_std_size(control.transaction_id);
    this->_data_io.write_std_long(control.query_id);
    this->_data_io.write_std_long(control.reference_address);
    this->_data_io.write_std_time(control.modify_time);
    this->_data_io.write_std_time(control.create_time);
  }
  
  void Memory::_load_control(const Pointer& pointer, Control& control)
  {
    uint8_t online_hexmap = 0;
    uint8_t offline_hexmap = 0;

    this->_hexmap_io.seekg(this->_pointer_hexmap_address(pointer), std::ios::beg);

    this->_hexmap_io.read_std_ubyte(online_hexmap);
    this->_hexmap_io.read_std_ubyte(offline_hexmap);
    this->_hexmap_io.read_std_ubyte();
    
    control.online_state = (RowState)(online_hexmap & (uint8_t)12);
    control.online_lock = (RowLock)(online_hexmap & (uint8_t)3);
    control.offline_state = (RowState)(offline_hexmap & (uint8_t)12);
    control.offline_lock = (RowLock)(offline_hexmap & (uint8_t)3);
    
    this->_data_io.seekg(pointer.address, std::ios::beg);

    this->_data_io.read_std_time(control.online_time);
    this->_data_io.read_std_size(control.transaction_id);
    this->_data_io.read_std_long(control.query_id);
    this->_data_io.read_std_long(control.reference_address);
    this->_data_io.read_std_time(control.modify_time);
    this->_data_io.read_std_time(control.create_time);
  }

  void Memory::_full_hexmap(int64_t address, int64_t count)
  {
    this->_hexmap_io.seekp(address, std::ios::beg);
    this->_hexmap_io.fill_n(count - 1, DATA_CHUNK);
    this->_hexmap_io.write_std_ubyte(DATA_STANDALONE_CHUNK);
  }

  void Memory::_full_hexmap(const Pointer& pointer)
  {
    int64_t address = this->_pointer_hexmap_data_address(pointer);
    int64_t count = this->_pointer_data_count(pointer);

    this->_hexmap_io.seekp(address, std::ios::beg);
    this->_hexmap_io.fill_n(count - 1, DATA_CHUNK);
    this->_hexmap_io.write_std_ubyte(DATA_STANDALONE_CHUNK + (pointer.size % CHUNK_SIZE));
  }

  void Memory::_free_hexmap(int64_t address, int64_t count)
  {
    this->_hexmap_io.seekp(address, std::ios::beg);
    this->_hexmap_io.fill_n(count - 1, FREE_CHUNK);
    this->_hexmap_io.write_std_ubyte(FREE_STANDALONE_CHUNK);
  }

  void Memory::_allocate_page(hashkey_ptr hash_key, int64_t address)
  {
    this->_full_hexmap(this->_pointer_hexmap_address(address), PAGEFILE_CONTROL_COUNT);
    this->_free_hexmap(this->_pointer_hexmap_address(address) + PAGEFILE_CONTROL_COUNT, this->_hbpp - PAGEFILE_CONTROL_COUNT);
    
    this->_data_io.seekp(address, std::ios::beg);
    this->_data_io.write((const char*)hash_key, HASHKEY_SIZE);

    this->_data_io.fill_n(this->_page_size - HASHKEY_SIZE, 0);
  }
  
  int64_t Memory::_allocate_new_page(hashkey_ptr hash_key)
  {
    auto pair_iter = this->_page_list.equal_range(FREE_HASHKEY);
    for (auto iter = pair_iter.first; iter != pair_iter.second; iter++) {

      this->_allocate_page(hash_key, iter->second);

      int64_t address = iter->second;
      this->_page_list.erase(iter);

      this->_page_list.insert({hash_key, address});
      
      return address;
    }

    this->_data_io.seekp(0, std::ios::end);
    int64_t start = this->_data_io.tellp();

    this->_allocate_page(hash_key, start);
    this->_page_list.insert({hash_key, start});
    this->_page_count++;

    return start;
  }

  Pointer Memory::_allocate(hashkey_ptr hash_key, int64_t size)
  {
    int64_t actual_size = this->_pointer_actual_size(size);
    if (actual_size > this->_page_size - PAGEFILE_CONTROL_SIZE - CONTROL_SIZE) {
      throw MemoryException("allocation overflow");
    }
    
    std::lock_guard<std::mutex> page_list_lock(this->_page_list_access);
    std::lock_guard<std::mutex> free_list_lock(this->_free_list_access);

    auto pair_iter = this->_free_list.equal_range(hash_key);
    for (auto iter = pair_iter.first; iter != pair_iter.second; iter++) {

      if (actual_size == iter->second.size) {

	Pointer pointer = iter->second;
	delete[] iter->first;
	this->_free_list.erase(iter);
	return pointer;

      } else if (actual_size < iter->second.size) {

	Pointer old_pointer = iter->second;
	Pointer new_pointer(old_pointer.hash_key, old_pointer.address, size);
	Pointer tmp_pointer(old_pointer.hash_key, old_pointer.address + actual_size, old_pointer.size - actual_size);

	delete[] iter->first;
	this->_free_list.erase(iter);
	this->_free_list_insert(new_pointer.hash_key, tmp_pointer);

	return new_pointer;
      }
      
    }

    auto hash_key_copy = new hashkey_t;
    std::memcpy(hash_key_copy, hash_key, HASHKEY_SIZE);
    
    int64_t start = this->_allocate_new_page(hash_key_copy);
      
    Pointer new_pointer(hash_key_copy, start + PAGEFILE_CONTROL_SIZE, size);
    Pointer tmp_pointer(hash_key_copy, start + PAGEFILE_CONTROL_SIZE + actual_size, this->_page_size - PAGEFILE_CONTROL_SIZE - actual_size);

    this->_free_list_insert(hash_key_copy, tmp_pointer);
    return new_pointer;
  }

  void Memory::_free_list_insert(hashkey_ptr hash_key, const Pointer& pointer)
  {
    auto hash_key_copy = new hashkey_t;
    std::memcpy(hash_key_copy, hash_key, HASHKEY_SIZE);

    this->_free_list.insert({hash_key_copy, pointer});
  }
  
  int64_t Memory::_free_prev_count()
  {
    uint8_t hexbyte;
    int64_t counter = 0;

    if (this->_hexmap_io.tellg() % this->_hbpp > 0) {
      this->_hexmap_io.seekg(-1, std::ios::cur);
      this->_hexmap_io.read_std_ubyte(hexbyte);
      if (hexbyte >> 7 == 0) { // free chunk found
	counter++;
	if (this->_hexmap_io.tellg() == 1) {
	  return counter;
	}
      } else if (this->_hexmap_io.tellg() == 1 || hexbyte >> 7 == 1) { // data chunk found
	return counter;
      } else {
	return 0;
      }
    }

    while (this->_hexmap_io.tellg() % this->_hbpp > 0) {
      this->_hexmap_io.seekg(-2, std::ios::cur);
      this->_hexmap_io.read_std_ubyte(hexbyte);
      if (hexbyte >> 7 == 0) { // free chunk found
	counter++;
	if (this->_hexmap_io.tellg() == 1) {
	  return counter;
	}
      } else if (this->_hexmap_io.tellg() == 1 || hexbyte >> 7 == 1) { // data chunk found
	return counter;
      } else {
	return 0;
      }
    }
    return 0;
  }
  
  int64_t Memory::_free_next_count()
  {
    uint8_t hexbyte;
    int64_t counter = 0;    

    while (this->_hexmap_io.tellg() % this->_hbpp > 0) {
      this->_hexmap_io.read_std_ubyte(hexbyte);
      if (hexbyte >> 7 == 0) { // free chunk found
	counter++;
	if (hexbyte >> 6 == 1) { // chunk is standalone
	  return counter;
	}
      } else {
	return 0;
      }
    }
    return 0;
  }

  void Memory::_free(const Pointer& pointer)
  {
    if (this->_watcher) {
      this->dba_watch("Free: (address: " + std::to_string(pointer.address) + ", size: " + std::to_string(pointer.size) + ")");
    }
    
    int64_t from = this->_pointer_hexmap_address(pointer);
    int64_t count = this->_pointer_actual_count(pointer);
    int64_t full_count = count;
    int64_t begin = from;

    std::lock_guard<std::mutex> page_list_lock(this->_page_list_access);
    std::lock_guard<std::mutex> free_list_lock(this->_free_list_access);
    
    this->_hexmap_io.seekp(from, std::ios::beg);
    int64_t prev_count = this->_free_prev_count();

    if (prev_count > 0) {
      full_count += prev_count;
      begin -= prev_count;

      auto pair_iter = this->_free_list.equal_range(pointer.hash_key);
      for (auto iter = pair_iter.first; iter != pair_iter.second; iter++) {

	if ((from - prev_count) * CHUNK_SIZE == iter->second.address) {
	  delete[] iter->first;
	  this->_free_list.erase(iter);
	  break;
	}
      }
    }
    
    this->_hexmap_io.seekp(from + count, std::ios::beg);
    int64_t next_count = this->_free_next_count();

    if (next_count > 0) {
      full_count += next_count;

      auto pair_iter = this->_free_list.equal_range(pointer.hash_key);
      for (auto iter = pair_iter.first; iter != pair_iter.second; ) {

	if ((from + count) * CHUNK_SIZE == iter->second.address) {
	  delete[] iter->first;
	  iter = this->_free_list.erase(iter);
	  break;
	} else {
	  iter++;
	}
      }
    }

    this->_free_hexmap(begin, full_count);

    if (this->_page_size == full_count * CHUNK_SIZE) {
      
      this->_allocate_page(FREE_HASHKEY, begin * CHUNK_SIZE);
      this->_page_list.insert({pointer.hash_key, begin * CHUNK_SIZE});
      
    } else {
      
      Pointer tmp_pointer(pointer.hash_key, begin * CHUNK_SIZE, full_count * CHUNK_SIZE);
      this->_free_list_insert(pointer.hash_key, tmp_pointer);
    }
  }

  void Memory::_free_key(hashkey_ptr hash_key)
  {
    std::lock_guard<std::mutex> page_list_lock(this->_page_list_access);

    auto pair_iter = this->_page_list.equal_range(hash_key);
    for (auto iter = pair_iter.first; iter != pair_iter.second; ) {

      this->_allocate_page(FREE_HASHKEY, iter->second);
      this->_page_list.insert({FREE_HASHKEY, iter->second});
      iter = this->_page_list.erase(iter);
    }
  }
    
  Pointer Memory::_pointer(hashkey_ptr hash_key, const int64_t& address)
  {
    uint8_t hex_byte = 0;
    int64_t size = 0;

    this->_hexmap_io.seekg(this->_pointer_hexmap_data_address(address), std::ios::beg);

    do {
      this->_hexmap_io.read_std_ubyte(hex_byte);

      // Reading past the end of the hexmap leaves the stream in a failed state
      // and every further read a no-op, so without this the loop spins forever
      // on a truncated or empty store rather than reporting the problem.
      if (!this->_hexmap_io.good()) {
	this->_hexmap_io.clear();
	throw MemoryException("hexmap ends inside the chunk at " + std::to_string(address));
      }

      if ( (hex_byte & (uint8_t)64) == 64) { // last chunk is standalone
	return Pointer(hash_key, address, size + (hex_byte & (uint8_t)31));
      }
      size += CHUNK_SIZE;
    } while (true);
  }

  Pointer Memory::_pointer(hashkey_ptr hash_key, const Long& address)
  {
    return this->_pointer(hash_key, address.value());
  }

  Pointer Memory::_pointer(hashkey_ptr hash_key, int64_t address, bool& is_data)
  {
    uint8_t hex_byte = 0;
    int64_t size = 0;

    this->_hexmap_io.seekg(std::floor((double)address / CHUNK_SIZE) + CONTROL_COUNT, std::ios::beg);
    
    do {
      this->_hexmap_io.read_std_ubyte(hex_byte);
      if ( (hex_byte & (uint8_t)64) == 64) { // last chunk is standalone
	is_data = ( (hex_byte & (uint8_t)128) == 128 ) ? true : false;
	return Pointer(hash_key, address, size + (hex_byte & (uint8_t)31) );
      }
      size += CHUNK_SIZE;
    } while (true);

    is_data = false;
  }

  Pointer Memory::_pointer(hashkey_ptr hash_key, int64_t address, bool& is_data, RowState& online_state, RowLock& online_lock)
  {
    uint8_t hex_byte = 0;
    int64_t size = 0;

    this->_hexmap_io.seekg(std::floor((double)address / CHUNK_SIZE), std::ios::beg);
    this->_hexmap_io.read_std_ubyte(hex_byte);

    online_state = (RowState)(hex_byte & (uint8_t)12);
    online_lock = (RowLock)(hex_byte & (uint8_t)3);    

    this->_hexmap_io.seekg(CONTROL_COUNT - 1, std::ios::cur);
    
    do {
      this->_hexmap_io.read_std_ubyte(hex_byte);
      if ( (hex_byte & (uint8_t)64) == 64) { // last chunk is standalone
	is_data = ( (hex_byte & (uint8_t)128) == 128 ) ? true : false;
        return  Pointer(hash_key, address, size + (hex_byte & (uint8_t)31) );
      }
      size += CHUNK_SIZE;
    } while (true);
  }

  bool Memory::_check_lock(RowLock lock, const Pointer& pointer, Control& control, lock_t* hexmap_lock, lock_t* data_lock)
  {
    if (this->_watcher) {
      this->dba_watch("Check Lock: (address: " + std::to_string(pointer.address) + ")");
    }
    
    bool do_read = true;
    int waited_ms = 0;
    do {
      this->_load_control(pointer, control);

      if (hexmap_lock) hexmap_lock->unlock();
      if (data_lock) data_lock->unlock();
	  
      if ( (control.online_lock & lock) != RowLock::NONE) { // pointer is locked

	if (control.transaction_id == Memory::transaction.id) { // lock acquired by itself

	  if (control.online_state == RowState::INSERTED && Memory::transaction.query_time > 0 && 
	      Memory::transaction.query_id == control.query_id) { // inserted from update cursor
	    do_read = false;
	  } else if (control.online_state == RowState::UPDATED || control.online_state == RowState::DELETED) {
	    do_read = false;
	  }
	  break;
	    
	} else {

	  if (control.online_state == RowState::INSERTED && control.reference_address > -1) {
	    do_read = false;
	    break;
	  } else  {
	    // Two sessions can each hold a lock the other is waiting for. Give up
	    // rather than spin forever: the caller rolls back and retries, which
	    // is what breaks the cycle.
	    if (Memory::lock_wait_timeout_ms > 0 && waited_ms >= Memory::lock_wait_timeout_ms) {
	      if (hexmap_lock) hexmap_lock->lock();
	      if (data_lock) data_lock->lock();
	      throw MemoryException("lock wait timeout");
	    }

	    std::this_thread::sleep_for(std::chrono::milliseconds(10));
	    waited_ms += 10;

	    if (hexmap_lock) hexmap_lock->lock();
	    if (data_lock) data_lock->lock();
	  }

	}

      } else {
	break; // no lock available
      }

    } while (true); // wait until lock is freed

    if (hexmap_lock) hexmap_lock->lock();
    if (data_lock) data_lock->lock();

    // The wait ends either because the writer committed or because it rolled
    // back. A row with nothing left online is only readable if it actually
    // landed: offline INSERTED or UPDATED. Rolled back rows are flagged DELETED
    // and blank rows are NONE, and reporting either would be a dirty read at
    // every level above READ UNCOMMITTED.
    if (do_read &&
	control.online_lock == RowLock::NONE &&
	control.online_state == RowState::NONE &&
	control.offline_state != RowState::INSERTED &&
	control.offline_state != RowState::UPDATED) {
      do_read = false;
    }

    return do_read;
  }

  void Memory::_cursor(hashkey_ptr hash_key, lock_t* hexmap_lock, lock_t* data_lock, std::function<bool (const Pointer&)> callback)
  {
    thread_local static uint8_t cursor_id = 0;
    if (++cursor_id >= MAX_CURSOR_COUNT) {
      throw MemoryException("maximum cursor count");
    }

    bool is_query = false;
    if (this->_initialized && Memory::transaction.query_time == 0) {
      is_query = true;
      Memory::transaction.query_id = (int64_t)std::rand();
      Memory::transaction.query_time = std::time(0);
    }

    uint8_t hex_byte = 0;
    int64_t hexmap_address = -1;
    int64_t pointer_address = -1;
    RowState online_state = RowState::NONE;
    RowState offline_state = RowState::NONE;
    RowLock online_lock = RowLock::NONE;
    Control control;

    auto pair_iter = this->_page_list.equal_range(hash_key);
    for (auto iter = pair_iter.first; iter != pair_iter.second; iter++) {

      hashkey_ptr page_hashkey = iter->first;
      int64_t page_address = iter->second;
      int64_t i = PAGEFILE_CONTROL_COUNT;

      while (i < this->_hbpp) {

	hexmap_address = this->_pointer_hexmap_address(page_address) + i;
	pointer_address = hexmap_address * CHUNK_SIZE;

	this->_hexmap_io.seekg(hexmap_address, std::ios::beg);
	
	this->_hexmap_io.read_std_ubyte(hex_byte);
	online_state = (RowState)(hex_byte & (uint8_t)12);
	online_lock = (RowLock)(hex_byte & (uint8_t)3);

	this->_hexmap_io.read_std_ubyte(hex_byte);
	offline_state = (RowState)(hex_byte & (uint8_t)12);

	this->_hexmap_io.seekg(CONTROL_COUNT - 2, std::ios::cur);

	i += CONTROL_COUNT;
	for (int64_t pointer_size = 0 ; i < this->_hbpp; pointer_size += CHUNK_SIZE) {

	  this->_hexmap_io.read_std_ubyte(hex_byte);
	  i++;
	  
	  if ( hex_byte != CONTROL_STANDALONE_CHUNK && (hex_byte & (uint8_t)64) == 64 ) { // last chunk is standalone
	    if ( hex_byte != CONTROL_CHUNK && (hex_byte & (uint8_t)128) == 128 ) { // last chunk is data

	      pointer_size += (hex_byte & (uint8_t)31);
	      Pointer pointer(page_hashkey, pointer_address, pointer_size);

	      bool do_callback = false;
	      bool break_while = false;
	      switch ((this->_initialized) ? Memory::transaction.isolation_level() : IsolationLevel::READ_UNCOMMITTED) {
		
	      case IsolationLevel::READ_UNCOMMITTED:
		if ( (offline_state == RowState::NONE     && online_state == RowState::INSERTED) ||
		     (offline_state == RowState::INSERTED && online_state == RowState::NONE    ) ){
		  do_callback = true;
		}
		break;
		
	      case IsolationLevel::READ_COMMITTED:
		if ((offline_state == RowState::NONE && online_state != RowState::NONE) || offline_state == RowState::INSERTED) {		  
		  do_callback = this->_check_lock(RowLock::EXCLUSIVE, pointer, control, hexmap_lock, data_lock);
		}		
		break;
		
	      case IsolationLevel::REPEATABLE_READ:
	      case IsolationLevel::SERIALIZABLE: // handled in transaction class as semaphore
		if ((offline_state == RowState::NONE && online_state != RowState::NONE) || offline_state == RowState::INSERTED) {		  
		  this->_load_control(pointer, control);
		  
		  if (online_lock == RowLock::NONE) {
		    control.online_state = RowState::NONE;
		    control.online_lock = RowLock::SHARED;
		    control.online_time = std::time(0);
		    control.transaction_id = Memory::transaction.id;    
		    control.query_id = Memory::transaction.query_id;
		    this->_dump_control(pointer, control);
		    this->_transaction_push(pointer);
		    do_callback = true;
		  } else {
		    do_callback = this->_check_lock(RowLock::EXCLUSIVE | RowLock::SHARED, pointer, control, hexmap_lock, data_lock);
		    if ( (!do_callback && control.transaction_id > 0 && control.transaction_id != Memory::transaction.id) ||
			 (do_callback && control.create_time > Memory::transaction.query_time)) {
		      // rollback_transaction_to() takes the same two mutexes this
		      // scan is already holding, so they have to be handed back
		      // before the retry or the scan deadlocks against itself.
		      if (hexmap_lock) hexmap_lock->unlock();
		      if (data_lock) data_lock->unlock();

		      this->rollback_transaction_to(Memory::transaction.query_time);
		      std::this_thread::sleep_for(std::chrono::milliseconds(10));

		      if (hexmap_lock) hexmap_lock->lock();
		      if (data_lock) data_lock->lock();

		      cursor_id--;
		      this->_cursor(hash_key, hexmap_lock, data_lock, callback);
		      return;
		    }
		  }
		}	
		break;
		
	      case IsolationLevel::SNAPSHOT:
		if ( (offline_state == RowState::NONE && online_state != RowState::NONE) 
		     || offline_state == RowState::INSERTED || offline_state == RowState::DELETED) {

		  do {
		    this->_load_control(pointer, control);

		    if (control.transaction_id > 0 && Memory::transaction.id == control.transaction_id) { // own lock
		      if (control.online_state == RowState::INSERTED)
			do_callback = true;
		      else
			do_callback = false;
		      break;
		    }

		    switch (control.offline_state) {
		    case RowState::NONE:
		      break_while = true;
		      break;
		      
		    case RowState::INSERTED:
		    case RowState::UPDATED:
		      if (control.create_time <= Memory::transaction.init_time) {
			do_callback = true;
			break_while = true;
		      } else if (control.reference_address > -1) {
			pointer = this->_pointer(hash_key, control.reference_address);
		      } else {
			break_while = true;
		      }
		      break;

		    case RowState::DELETED:
		      if (control.modify_time > Memory::transaction.init_time) {
			do_callback = true;
			break_while = true;
		      } else if (control.reference_address > -1) {
			pointer = this->_pointer(hash_key, control.reference_address);
		      } else {
			break_while = true;
		      }
		      break;
		      
		    default:
		      throw MemoryException("invalid row state");
		    } // switch	
		  } while (!break_while);

		} // if
		break;

	      default:
		throw MemoryException("invalid isolation level");
	      }
    
	      if (do_callback) {

		if (hexmap_lock) hexmap_lock->unlock();
		if (data_lock) data_lock->unlock();
		
		if ( !callback( pointer ) ) {
		  i = this->_hbpp;
		}

		if (hexmap_lock) hexmap_lock->lock();
		if (data_lock) data_lock->lock();

	      }
	      
	    } // if data
	    break;
	  } // if standalone
	} // for
      } // while
    }// for pages

    if (is_query) {
      Memory::transaction.query_id = 0;
      Memory::transaction.query_time = 0;
    }

    --cursor_id;
  }

  void Memory::_control_insert(BaseTable* object)
  {
    if (this->_watcher) {
      this->dba_watch("Control Insert: (address: " + std::to_string(object->pointer.address) + ")");
    }
    
    Control control;

    control.online_state = RowState::INSERTED;
    control.online_lock = RowLock::EXCLUSIVE;

    control.online_time = std::time(0);
    control.transaction_id = Memory::transaction.id;
    control.query_id = Memory::transaction.query_id;
    
    this->_dump_control(object->pointer, control);
  }
  
  void Memory::_control_update(BaseTable* old_object, BaseTable* new_object, lock_t* hexmap_lock, lock_t* data_lock)
  {
    if (this->_watcher) {
      this->dba_watch("Control Update: (address: " + std::to_string(old_object->pointer.address) + ")");
    }

    Control control;
    
    if (!this->_check_lock(RowLock::EXCLUSIVE, old_object->pointer, control, hexmap_lock, data_lock))
      return;

    control.online_state = RowState::UPDATED;
    control.online_lock = RowLock::EXCLUSIVE;

    control.online_time = std::time(0);
    control.transaction_id = Memory::transaction.id;
    control.query_id = Memory::transaction.query_id;
    
    this->_dump_control(old_object->pointer, control);
    
    control = Control();
    
    control.online_state = RowState::INSERTED;
    control.online_lock = RowLock::EXCLUSIVE;
    
    control.online_time = std::time(0);
    control.transaction_id = Memory::transaction.id;
    control.query_id = Memory::transaction.query_id;
    control.reference_address = old_object->pointer.address;
    
    this->_dump_control(new_object->pointer, control);
  }
  
  void Memory::_control_delete(BaseTable* object, lock_t* hexmap_lock, lock_t* data_lock)
  {
    if (this->_watcher) {
      this->dba_watch("Control Delete: (address: " + std::to_string(object->pointer.address) + ")");
    }

    Control control;
 
    if (!this->_check_lock(RowLock::EXCLUSIVE, object->pointer, control, hexmap_lock, data_lock))
      return;
    
    control.online_state = RowState::DELETED;
    control.online_lock = RowLock::EXCLUSIVE;

    control.online_time = std::time(0);
    control.transaction_id = Memory::transaction.id;
    control.query_id = Memory::transaction.query_id;
    
    this->_dump_control(object->pointer, control);
  }
  
  void Memory::_transaction_push(const Pointer& pointer)
  {
    if (this->_watcher) {
      this->dba_watch("Transaction Push: (address: " + std::to_string(pointer.address) + ")");
    }

    Memory::transaction.context.emplace_back(std::time(0), pointer);
  }

  void Memory::_commit_pointer(const Pointer& pointer, time_t commit_time)
  {
    if (this->_watcher) {
      this->dba_watch("Commit Pointer: (address: " + std::to_string(pointer.address) + ")");
    }

    Control control;
    this->_load_control(pointer, control);
  
    switch (control.online_state) {
    case RowState::NONE:
      break;
    case RowState::INSERTED:
      control.offline_state = RowState::INSERTED;
      control.create_time = commit_time;
      break;
    case RowState::UPDATED:
      control.offline_state = RowState::UPDATED;
      control.modify_time = commit_time;
      break;
    case RowState::DELETED:
      control.offline_state = RowState::DELETED;
      control.modify_time = commit_time;
      break;
    }

    control.online_state = RowState::NONE;
    control.online_lock = RowLock::NONE;

    control.online_time = (time_t)(0);
    control.transaction_id = 0;
    control.query_id = 0;

    this->_dump_control(pointer, control);
  }

  void Memory::_rollback_pointer(const Pointer& pointer)
  {
    if (this->_watcher) {
      this->dba_watch("Rollback Pointer: (address: " + std::to_string(pointer.address) + ")");
    }

    Control control;
    this->_load_control(pointer, control);

    switch (control.online_state) {
    case RowState::NONE:
    case RowState::INSERTED:
    case RowState::UPDATED:
    case RowState::DELETED:
      control.online_state = RowState::NONE;
      control.online_lock = RowLock::NONE;
      control.online_time = (time_t)0;
      control.transaction_id = 0;
      control.query_id = 0;

      // A row that was never committed is flagged deleted rather than freed.
      // Rows are never reclaimed: keeping the chunk is what lets the store be
      // read back at an earlier point in time, and it stops a rollback handing
      // a chunk back to the allocator while another session is still working
      // next to it.
      if (control.offline_state == RowState::NONE) {
	control.offline_state = RowState::DELETED;
	control.modify_time = std::time(0);
      }

      this->_dump_control(pointer, control);
      break;
    }
  }

  void Memory::_write_transaction(size_t transaction_id, time_t commit_time) // Transaction Durability
  {
    size_t length = sizeof(size_t) + sizeof(time_t);
    char buffer[length];
    size_t* transaction_id_ptr = reinterpret_cast<size_t*>(&buffer[0]);
    time_t* commit_time_ptr = reinterpret_cast<time_t*>(&buffer[sizeof(size_t)]);
    *transaction_id_ptr = transaction_id;
    *commit_time_ptr = commit_time;

    this->_data_io.seekp(this->_pointer_data_address(Memory::transaction.pointer), std::ios::beg);
    this->_data_io.write(buffer, length);
  }
  
  void Memory::begin_transaction()
  {
    // The thread_local Transaction constructor calls straight back into this
    // function, and the mutexes below are not recursive. The outer call does the
    // work; the nested one returns and lets the constructor finish.
    struct ReentryGuard
    {
      bool& flag;
      explicit ReentryGuard(bool& f) : flag(f) { flag = true; }
      ~ReentryGuard() { flag = false; }
    };

    static thread_local bool beginning = false;
    if (beginning) return;
    ReentryGuard guard(beginning);

    // Allocating the transaction record touches the hexmap and the data file
    // exactly as commit and rollback do, so it needs the same two mutexes.
    // Without them concurrent sessions raced here and lost each other's pages.
    std::lock_guard<std::mutex> hexmap_lock(this->_hexmap_access);
    std::lock_guard<std::mutex> data_lock(this->_data_access);

    //                                                                   transaction_id , commit_time
    Memory::transaction.initialize(this->_allocate(TRANSACTIONS_HASHKEY, sizeof(size_t) + sizeof(time_t)));

    if (this->_watcher) {
      this->dba_watch("Begin Transaction: ()");
    }

    Control control;
    control.offline_state = RowState::INSERTED;
    this->_dump_control(Memory::transaction.pointer, control);

    this->_full_hexmap(Memory::transaction.pointer);
    this->_write_transaction(Memory::transaction.id, (time_t)0);
  }

  void Memory::commit_transaction()
  {
    if (this->_watcher) {
      this->dba_watch("Commit Transaction: ()");
    }

    std::lock_guard<std::mutex> hexmap_lock(this->_hexmap_access);
    std::lock_guard<std::mutex> data_lock(this->_data_access);    

    time_t commit_time = std::time(0);

    this->_write_transaction(Memory::transaction.id, commit_time);

    for (const auto& pair : Memory::transaction.context) {
      this->_commit_pointer(pair.second, commit_time);
    }
    Memory::transaction.context.clear();

    this->_write_transaction(Memory::transaction.id, (time_t)0);

    this->_hexmap_io.flush();
    this->_data_io.flush();
  }
  
  void Memory::commit_transaction_until(time_t)
  {

  }

  void Memory::rollback_transaction()
  {
    if (this->_watcher) {
      this->dba_watch("Rollback Transaction: ()");
    }

    std::lock_guard<std::mutex> hexmap_lock(this->_hexmap_access);
    std::lock_guard<std::mutex> data_lock(this->_data_access);    

    auto e_iter = Memory::transaction.context.crend();
    for (auto iter = Memory::transaction.context.crbegin(); iter != e_iter; iter++) {
      this->_rollback_pointer(iter->second);
    }
    Memory::transaction.context.clear();

    this->_free(Memory::transaction.pointer);
    
    this->_hexmap_io.flush();
    this->_data_io.flush();
  }
  
  void Memory::rollback_transaction_to(time_t time)
  {
    if (this->_watcher) {
      this->dba_watch("Rollback Transaction to: (" + std::string(asctime(gmtime(&time))) + ")");
    }

    std::lock_guard<std::mutex> hexmap_lock(this->_hexmap_access);
    std::lock_guard<std::mutex> data_lock(this->_data_access);    

    for (auto iter = Memory::transaction.context.crbegin(); iter != Memory::transaction.context.crend(); ) {
      if (iter->first >= time) {
	this->_rollback_pointer(iter->second);
	iter = typename decltype(Transaction::context)::reverse_iterator(Memory::transaction.context.erase(--iter.base()));
      } else {
	break;
      }
    }
    
    this->_hexmap_io.flush();
    this->_data_io.flush();
  }

  size_t Memory::transaction_id()
  {
    return Memory::transaction.id;
  }

  int64_t Memory::_pointer_data_count(int64_t size)
  {
    return std::ceil((double)size / CHUNK_SIZE);
  }
  
  int64_t Memory::_pointer_actual_count(int64_t size)
  {
    return this->_pointer_data_count(size) + CONTROL_COUNT;
  }
  
  int64_t Memory::_pointer_round_size(int64_t size)
  {
    return this->_pointer_data_count(size) * CHUNK_SIZE;
  }
  
  int64_t Memory::_pointer_actual_size(int64_t size)
  {
    return this->_pointer_actual_count(size) * CHUNK_SIZE;
  }
  
  int64_t Memory::_pointer_hexmap_address(int64_t address)
  {
    return (address / CHUNK_SIZE);
  }
 
  int64_t Memory::_pointer_hexmap_data_address(int64_t address)
  {
    return (address / CHUNK_SIZE) + CONTROL_COUNT;
  }
 
  int64_t Memory::_pointer_data_address(int64_t address)
  {
    return (address + CONTROL_SIZE);
  }

  int64_t Memory::_pointer_data_count(const Pointer& pointer)
  {
    return std::ceil((double)pointer.size / CHUNK_SIZE);
  }
  
  int64_t Memory::_pointer_actual_count(const Pointer& pointer)
  {
    return this->_pointer_data_count(pointer) + CONTROL_COUNT;
  }
  
  int64_t Memory::_pointer_round_size(const Pointer& pointer)
  {
    return this->_pointer_data_count(pointer) * CHUNK_SIZE;
  }
  
  int64_t Memory::_pointer_actual_size(const Pointer& pointer)
  {
    return this->_pointer_actual_count(pointer) * CHUNK_SIZE;
  }
  
  int64_t Memory::_pointer_hexmap_address(const Pointer& pointer)
  {
    return pointer.address / CHUNK_SIZE;
  }
 
  int64_t Memory::_pointer_hexmap_data_address(const Pointer& pointer)
  {
    return (pointer.address / CHUNK_SIZE) + CONTROL_COUNT;
  }
 
  int64_t Memory::_pointer_data_address(const Pointer& pointer)
  {
    return pointer.address + CONTROL_SIZE;
  }

  void Memory::dba_pagefiles(binarystream& stream)
  {
    std::lock_guard<std::mutex> page_list_lock(this->_page_list_access);
    stream.write_std_size(this->_page_list.size());
    for (auto iter = this->_page_list.begin(); iter != this->_page_list.end(); iter++) {
      stream.write_std_string(Utility::octet_as_hex(iter->first, HASHKEY_SIZE));
      stream.write_std_long(iter->second);
    }
  }

  void Memory::dba_pointers(binarystream& stream)
  {
    int64_t pagefile_start;
    stream.read_std_long(pagefile_start);

    if (pagefile_start > this->_page_count * this->_page_size) {
      stream.write_std_bool(false);
      return;
    }

    int64_t begin_address, end_address;
    bool is_data;
    Control control;
    uint8_t hex_byte;
    int64_t actual_size, size;

    stream.write_std_bool(true);
    stream.write_std_ubyte(2);
    stream.write_std_long(pagefile_start);
    stream.write_std_long(PAGEFILE_CONTROL_SIZE);
    stream.write_std_long(PAGEFILE_CONTROL_SIZE);

    begin_address = pagefile_start + PAGEFILE_CONTROL_SIZE;
    end_address = pagefile_start + this->_page_size;
      
    while (begin_address < end_address) {

      this->_data_io.seekg(begin_address, std::ios::beg);

      this->_data_io.read_std_time(control.online_time);
      this->_data_io.read_std_size(control.transaction_id);
      this->_data_io.read_std_long(control.query_id);
      this->_data_io.read_std_long(control.reference_address);
      this->_data_io.read_std_time(control.modify_time);
      this->_data_io.read_std_time(control.create_time);

      this->_hexmap_io.seekg(this->_pointer_hexmap_address(begin_address), std::ios::beg);
      
      this->_hexmap_io.read_std_ubyte(hex_byte);
      control.online_state = (RowState)(hex_byte & (uint8_t)12);
      control.online_lock = (RowLock)(hex_byte & (uint8_t)3);    
      
      this->_hexmap_io.read_std_ubyte(hex_byte);
      control.offline_state = (RowState)(hex_byte & (uint8_t)12);
      control.offline_lock = (RowLock)(hex_byte & (uint8_t)3);    

      this->_hexmap_io.seekg(CONTROL_COUNT - 2, std::ios::cur);

      size = 0;
      actual_size = CONTROL_SIZE;

      do {

	this->_hexmap_io.read_std_ubyte(hex_byte);
	if ( (hex_byte & (uint8_t)64) == 64) { // last chunk is standalone

	  is_data = ( (hex_byte & (uint8_t)128) == 128 ) ? true : false;

	  stream.write_std_bool(true);
	  stream.write_std_ubyte(is_data); 
	  stream.write_std_long(begin_address);
	  stream.write_std_long(size + (hex_byte & (uint8_t)31));
	  stream.write_std_long(actual_size + CHUNK_SIZE);

	  if (is_data) {
	    stream.write_std_ubyte((uint8_t)control.online_state);
	    stream.write_std_ubyte((uint8_t)control.online_lock);
	    stream.write_std_ubyte((uint8_t)control.offline_state);
	    stream.write_std_ubyte((uint8_t)control.offline_lock);
	    stream.write_std_time(control.online_time);
	    stream.write_std_size(control.transaction_id);
	    stream.write_std_long(control.query_id);
	    stream.write_std_long(control.reference_address);
	    stream.write_std_time(control.modify_time);
	    stream.write_std_time(control.create_time);
	  }

	  actual_size += CHUNK_SIZE;
	  break;
	}

	size += CHUNK_SIZE;
	actual_size += CHUNK_SIZE;
      } while (true);

      begin_address += actual_size;
    }

    stream.write_std_bool(false);
  }

  void Memory::dba_attach_watcher(binarystream* watcher)
  {
    if (this->_watcher)
      delete this->_watcher;
    this->_watcher = watcher;
  }

  void Memory::dba_detach_watcher()
  {
    if (this->_watcher)
      delete this->_watcher;
    this->_watcher = nullptr;
  }

  void Memory::dba_watch(std::string&& data)
  {
    try {
      this->_watcher->write_std_size(Memory::transaction.id);
      this->_watcher->write_std_string(data);
    } catch (...) {
      this->_watcher = nullptr;
    }
  }

  Memory::~Memory()
  {

  }
  
}
