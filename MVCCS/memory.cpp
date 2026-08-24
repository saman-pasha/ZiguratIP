#include <utility>
#include <vector>
#include "memory.hpp"
#include "memoryexception.hpp"
#include "filestream.hpp"
#include <cmath>
#include <thread>
#include <chrono>
#include <atomic>
#include <iterator>
#include "shahelper.hpp"
#include "control.hpp"
#include "basetable.hpp"
#include "transaction.hpp"
#include "utility.hpp"
#include "tcpstream.hpp"


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

  static inline int64_t _size_remainder(uint8_t hex_byte)
  {
    int64_t r = (int64_t)(hex_byte & (uint8_t)31);
    if ( (hex_byte & (uint8_t)128) == 128 && r == 0 ) return Memory::CHUNK_SIZE;
    return r;
  }

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

  // ---- the live-transaction registry ---------------------------------------
  // What it is for is in memory.hpp, above the declaration.

  std::mutex Memory::_live_txn_mutex;
  std::unordered_set<size_t> Memory::_live_txn_ids;

  // The thread's slot: the one id this thread has begun and not yet ended.
  // begin_transaction retiring it before registering the fresh id is what
  // makes an abandoned transaction's debris breakable.
  static thread_local size_t _thread_live_txn = 0;

  bool Memory::transaction_live(size_t transaction_id)
  {
    std::lock_guard<std::mutex> guard(Memory::_live_txn_mutex);
    return Memory::_live_txn_ids.find(transaction_id) != Memory::_live_txn_ids.end();
  }

  void Memory::transaction_retire(size_t transaction_id)
  {
    std::lock_guard<std::mutex> guard(Memory::_live_txn_mutex);
    Memory::_live_txn_ids.erase(transaction_id);
    if (_thread_live_txn == transaction_id) _thread_live_txn = 0;
  }

  void Memory::_transaction_register(size_t fresh_id)
  {
    std::lock_guard<std::mutex> guard(Memory::_live_txn_mutex);
    if (_thread_live_txn != 0) Memory::_live_txn_ids.erase(_thread_live_txn);
    Memory::_live_txn_ids.insert(fresh_id);
    _thread_live_txn = fresh_id;
  }

  bool Memory::HashKeyComparer::operator()(hashkey_ptr src, hashkey_ptr dst) const
  {
    return (std::memcmp(src, dst, HASHKEY_SIZE) < 0);
  }

  thread_local Transaction Memory::transaction;

  // ---- Memory::Streams -----------------------------------------------------
  // What it is for is in memory.hpp, above the declaration.

  thread_local Memory::Streams* Memory::Streams::_held = nullptr;

  // ---- the shared read side ------------------------------------------------
  // What it is for is in memory.hpp, above reader_paths. Ported from the
  // Cicili engine; the shapes and the reasons are its, line for line.

  thread_local int Memory::_tl_read_mode = 0;
  std::atomic<uint64_t> Memory::_reader_epoch(0);
  thread_local uint64_t Memory::_tl_rd_epoch = 0;
  thread_local binarystream* Memory::_tl_rd_hex = nullptr;
  thread_local binarystream* Memory::_tl_rd_data = nullptr;
  thread_local int Memory::_tl_want_shared = 0;

  void Memory::reader_paths(const std::string& hexmap_path, const std::string& data_path)
  {
    this->_rd_hex_path = hexmap_path;
    this->_rd_data_path = data_path;
    this->_reader_stamp = ++Memory::_reader_epoch;
  }

  // A shared guard is granted only to a read whose isolation writes nothing:
  // REPEATABLE READ and SERIALIZABLE stamp shared row locks as they scan --
  // the lesson of the reverted first attempt at this -- so the lock mode
  // follows the level.
  bool Memory::_reader_eligible()
  {
    if (!this->_initialized) return false;
    if (this->_rd_hex_path.empty()) return false;
    const IsolationLevel level = Memory::transaction.isolation_level();
    if (level == IsolationLevel::REPEATABLE_READ || level == IsolationLevel::SERIALIZABLE)
      return false;
    return true;
  }

  void Memory::_reader_ensure()
  {
    if (Memory::_tl_rd_hex != nullptr && Memory::_tl_rd_epoch == this->_reader_stamp)
      return;
    delete Memory::_tl_rd_hex;
    delete Memory::_tl_rd_data;
    Memory::_tl_rd_hex = new Zigurat::filestream(this->_rd_hex_path, std::ios::in);
    Memory::_tl_rd_data = new Zigurat::filestream(this->_rd_data_path, std::ios::in);
    Memory::_tl_rd_epoch = this->_reader_stamp;
  }

  binarystream& Memory::_hex_in()
  {
    if (Memory::_tl_read_mode == 0 || Memory::Streams::held() != nullptr)
      return this->_hexmap_io;
    this->_reader_ensure();
    return *Memory::_tl_rd_hex;
  }

  binarystream& Memory::_data_in()
  {
    if (Memory::_tl_read_mode == 0 || Memory::Streams::held() != nullptr)
      return this->_data_io;
    this->_reader_ensure();
    return *Memory::_tl_rd_data;
  }

  // Built deferred and then locked through lock(), so that publishing happens
  // in exactly one place. A guard on a thread that is already holding them owns
  // nothing at all: it never locks, never unlocks, and never publishes, so the
  // real holder's state is left alone. The constructor consumes the
  // want-shared flag and grants it only to the outermost guard of an eligible
  // reader; a write guard wanted under a shared hold is trapped in lock()
  // rather than deadlocking on the rwlock.
  Memory::Streams::Streams(Memory* memory)
    : _memory(memory),
      _mine(false),
      _locked(false),
      _shared(false)
  {
    const int want = Memory::_tl_want_shared;
    Memory::_tl_want_shared = 0;

    if (_held != nullptr) {
      this->_mine = false;                    // nested under a write hold: no-op
    } else if (Memory::_tl_read_mode == 1) {
      this->_mine = (want != 1);              // nested under a shared hold: no-op
                                              // -- unless a WRITE is wanted,
                                              // which lock() surfaces as a bug
    } else {
      this->_mine = true;                     // outermost
      if (want == 1 && memory->_reader_eligible())
	this->_shared = true;
    }

    this->lock();
  }

  Memory::Streams::~Streams()
  {
    this->unlock();
  }

  bool Memory::Streams::mine() const
  {
    return this->_mine;
  }

  void Memory::Streams::lock()
  {
    if (!this->_mine || this->_locked) return;

    if (this->_shared) {
      pthread_rwlock_rdlock(&this->_memory->_streams_rw);
      Memory::_tl_read_mode = 1;
    } else {
      // A write guard under a held shared one would deadlock on the rwlock --
      // surface the bug instead of wedging on it.
      if (Memory::_tl_read_mode == 1)
	throw MemoryException("an exclusive streams guard under a shared one");
      pthread_rwlock_wrlock(&this->_memory->_streams_rw);
      _held = this;
    }
    this->_locked = true;
  }

  void Memory::Streams::unlock()
  {
    if (!this->_mine || !this->_locked) return;

    if (this->_shared) {
      // WITHDRAWN BEFORE RELEASING: never claim a hold already given back.
      Memory::_tl_read_mode = 0;
      this->_locked = false;
      pthread_rwlock_unlock(&this->_memory->_streams_rw);
    } else {
      // Every write is in the FILE before the guard is released, or a private
      // reader taken the next instant would miss the bytes still sitting in
      // the canonical streams' buffers.
      this->_memory->_hexmap_io.flush();
      this->_memory->_data_io.flush();
      _held = nullptr;
      this->_locked = false;
      pthread_rwlock_unlock(&this->_memory->_streams_rw);
    }
  }

  Memory::Streams* Memory::Streams::held()
  {
    return _held;
  }

  // ---- the version clock ---------------------------------------------------

  time_t Memory::version_time()
  {
    static std::atomic<int64_t> last(0);

    const int64_t now = (int64_t)std::chrono::duration_cast<std::chrono::microseconds>
      (std::chrono::system_clock::now().time_since_epoch()).count();

    // Strictly increasing, whatever the clock says. Two commits inside the same
    // microsecond -- or a wall clock that steps backwards -- must not be able to
    // produce the same stamp twice, because a stamp is what tells one version of
    // a row from the next.
    int64_t previous = last.load(std::memory_order_relaxed);
    int64_t next = 0;
    do {
      next = (now > previous) ? now : (previous + 1);
    } while (!last.compare_exchange_weak(previous, next, std::memory_order_relaxed));

    return (time_t)next;
  }

  // ---- Memory::Statement ---------------------------------------------------

  Memory::Statement::Statement(Memory* memory)
    : _mine(false)
  {
    // Only the outermost one starts a statement. An inner cursor -- a scan
    // driving an index lookup, say -- belongs to the statement already running,
    // and taking a fresh time for it would be the very interleaving this exists
    // to stop.
    //
    // Nothing before the store has finished loading has a statement either: the
    // walk _initialize does reads at READ UNCOMMITTED and predates the machinery
    // this feeds.
    if (memory == nullptr || !memory->_initialized) return;
    if (Memory::transaction.query_time != 0) return;

    this->_mine = true;

    // Written into the store and read back, so that rows THIS query inserted can
    // be told from rows that were already there and an update cursor does not
    // walk over its own output. std::rand() with nobody calling std::srand
    // returns the same sequence on every start of the process.
    uint64_t drawn = 0;
    Utility::random_bytes((uint8_t*)&drawn, sizeof(drawn));
    Memory::transaction.query_id = (int64_t)(drawn >> 1);   // stays positive

    Memory::transaction.query_time = Memory::version_time();
  }

  Memory::Statement::~Statement()
  {
    if (!this->_mine) return;
    Memory::transaction.query_id = 0;
    Memory::transaction.query_time = 0;
  }

  bool Memory::Statement::mine() const
  {
    return this->_mine;
  }

  Memory::Memory(binarystream& hexmap_io, binarystream& data_io, int64_t page_size)
    : _hexmap_io(hexmap_io), _data_io(data_io), _page_size(page_size), _hbpp(page_size / CHUNK_SIZE)
  {
    // Writer-preferring, or a steady stream of readers starves every writer:
    // with the default policy a commit could wait behind readers indefinitely
    // while new readers kept joining ahead of it.
    {
      pthread_rwlockattr_t rwa;
      pthread_rwlockattr_init(&rwa);
#ifdef PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP
      pthread_rwlockattr_setkind_np(&rwa, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif
      pthread_rwlock_init(&this->_streams_rw, &rwa);
      pthread_rwlockattr_destroy(&rwa);
    }

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
    
    this->_cursor(TRANSACTIONS_HASHKEY, Streams::held(), [&] (const Pointer pointer) -> bool {

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

	// FREE OR RECORD IS DECIDED FROM THE FIRST HEXMAP BYTE, before any
	// parsing. _pointer seeks CONTROL_COUNT bytes in and measures what
	// follows, which is right for a record and reads straight through a
	// SHORT free run into the record behind it -- the comment below knew
	// this, but the is_data answer gating the two branches came out of
	// that same read-through: a one-chunk free run parsed as a 64-byte
	// "record", the walk went one phase out of step, read data codes as
	// control bytes, and rolled back live rows it mistook for uncommitted
	// -- measured as a live index node zeroed at every eleventh run's
	// restart. A record's first chunk is a control chunk and always
	// carries the high bit; a free chunk never does. One byte decides.
	uint8_t head_byte = 0;
	this->_hexmap_io.seekg(this->_pointer_hexmap_address(begin_address), std::ios::beg);
	this->_hexmap_io.read_std_ubyte(head_byte);
	if (!this->_hexmap_io.good()) { this->_hexmap_io.clear(); break; }
	is_data = (head_byte & (uint8_t)128) == 128;

	Pointer pointer;
	if (is_data)
	  pointer = this->_pointer(iter->first, begin_address, is_data, online_state, online_lock);

	if (is_data) {
	  if (online_lock != RowLock::NONE) {

	    // hashkey_ptr is const uint8_t*, so streaming it picks the overload
	    // that prints a C string: strlen ran off the end of a twenty octet
	    // hash and kept going to the first zero byte, printing whatever it
	    // passed. AddressSanitizer catches it on the first uncommitted
	    // pointer found at startup.
	    std::cout << "uncommitted_pointer: "
		      << Utility::octet_as_hex(iter->first, sizeof(hashkey_t))
		      << "," << begin_address << std::endl;
    
	    this->_load_control(pointer, control);

	    std::map<size_t, time_t>::iterator iter = transactions.end();
	    if ((iter = transactions.find(control.transaction_id)) != transactions.end() && iter->second > (time_t)0) {
	      this->_commit_pointer(pointer, iter->second);
	    } else {
	      this->_rollback_pointer(pointer);
	    }
	  }	  
	  begin_address += this->_pointer_actual_size(pointer);

	} else {

	  // A FREE RUN IS NOT A RECORD, and _pointer cannot describe one. It
	  // seeks past CONTROL_COUNT control chunks and measures what follows,
	  // which is right for a record and wrong for a hole: a hole has no
	  // control block, so on a run shorter than CONTROL_COUNT + 1 chunks it
	  // reads straight through into the record behind it and answers a size
	  // covering both. That size went into the free list, and the next
	  // allocation from it wrote over a live row.
	  //
	  // So the run is measured from the hexmap, which is the only thing that
	  // knows where it ends: chunks with the high bit clear, ending after the
	  // one carrying the standalone bit.
	  int64_t run = this->_free_run_count(begin_address, end_address);

	  // Zero would not advance, and a page walk that does not advance is a
	  // hang at startup. It cannot happen -- is_data is false, so the chunk
	  // here has its high bit clear and counts -- but this loop is not the
	  // place to find out that something else was assumed wrongly.
	  if (run <= 0) break;

	  Pointer free_pointer(iter->first, begin_address, run * CHUNK_SIZE);
	  this->_free_list_insert(iter->first, free_pointer);

	  // AND KEEP WALKING. This used to break, which is only correct if free
	  // space is always the page's tail. It is not: a rolled-back transaction
	  // frees its record wherever it sits, and everything allocated after it
	  // is then behind a hole. Stopping there left those records unseen, the
	  // free space past them unregistered, and the allocator's view of the
	  // page disagreeing with what was in it.
	  begin_address += run * CHUNK_SIZE;
	}
      }

      std::cout << "memory loading... " << (100 * i) / this->_page_count << "%\r";
      std::cout.flush();
    }

    std::cout << "memory loading... 100%" << std::endl;
    
    if (transactions.size() > 0) {
      // COLLECTED FIRST, FREED AFTER, which is the rule truncate already
      // follows and says why: _free moves entries between the page list and the
      // free list, and it can hand a whole page back to FREE_HASHKEY -- so
      // freeing from inside the walk mutates what the walk is standing on.
      std::list<Pointer> spent;
      this->_cursor(TRANSACTIONS_HASHKEY, Streams::held(), [&] (const Pointer& pointer) -> bool {
	  spent.push_back(pointer);
	  return true;
	});

      for (const Pointer& pointer : spent) {
	this->_free(pointer);
      }
      std::cout << "memory restore ends" << std::endl;
    }

    this->_initialized = true;
  }

  void Memory::_dump_control(const Pointer& pointer, const Control& control)
  {
    uint8_t online_hexmap = CONTROL_CHUNK + (uint8_t)control.online_state + (uint8_t)control.online_lock;
    uint8_t offline_hexmap = CONTROL_CHUNK + (uint8_t)control.offline_state + (uint8_t)control.offline_lock;
    
    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_hexmap_io.good()) this->_hexmap_io.clear();
    this->_hexmap_io.seekp(this->_pointer_hexmap_address(pointer), std::ios::beg);

    this->_hexmap_io.write_std_ubyte(online_hexmap);
    this->_hexmap_io.write_std_ubyte(offline_hexmap);
    this->_hexmap_io.write_std_ubyte(CONTROL_STANDALONE_CHUNK);

    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_data_io.good()) this->_data_io.clear();
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

    // Through the read accessors, because a shared-guard reader judges rows
    // through this function too -- and a stream a failed read has poisoned
    // answers nothing until it is cleared, which for a thread's PRIVATE
    // stream would turn every later judgment on that thread into garbage.
    binarystream& hin = this->_hex_in();
    binarystream& din = this->_data_in();
    if (!hin.good()) hin.clear();
    if (!din.good()) din.clear();

    hin.seekg(this->_pointer_hexmap_address(pointer), std::ios::beg);

    hin.read_std_ubyte(online_hexmap);
    hin.read_std_ubyte(offline_hexmap);
    hin.read_std_ubyte();

    control.online_state = (RowState)(online_hexmap & (uint8_t)12);
    control.online_lock = (RowLock)(online_hexmap & (uint8_t)3);
    control.offline_state = (RowState)(offline_hexmap & (uint8_t)12);
    control.offline_lock = (RowLock)(offline_hexmap & (uint8_t)3);

    din.seekg(pointer.address, std::ios::beg);

    din.read_std_time(control.online_time);
    din.read_std_size(control.transaction_id);
    din.read_std_long(control.query_id);
    din.read_std_long(control.reference_address);
    din.read_std_time(control.modify_time);
    din.read_std_time(control.create_time);
  }

  void Memory::_full_hexmap(int64_t address, int64_t count)
  {
    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_hexmap_io.good()) this->_hexmap_io.clear();
    this->_hexmap_io.seekp(address, std::ios::beg);
    this->_hexmap_io.fill_n(count - 1, DATA_CHUNK);
    this->_hexmap_io.write_std_ubyte(DATA_STANDALONE_CHUNK);
  }

  void Memory::_full_hexmap(const Pointer& pointer)
  {
    int64_t address = this->_pointer_hexmap_data_address(pointer);
    int64_t count = this->_pointer_data_count(pointer);

    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_hexmap_io.good()) this->_hexmap_io.clear();
    this->_hexmap_io.seekp(address, std::ios::beg);
    this->_hexmap_io.fill_n(count - 1, DATA_CHUNK);
    this->_hexmap_io.write_std_ubyte(DATA_STANDALONE_CHUNK + (pointer.size % CHUNK_SIZE));
  }

  // How many chunks the free run at ADDRESS spans, read from the hexmap.
  //
  // A run is bytes with the high bit clear, ending after the one with the
  // standalone bit -- exactly what _free_hexmap writes. It stops at END_ADDRESS
  // whatever the bytes say, because a page's free space cannot extend past the
  // page, and a hexmap that claims otherwise must not send the walk into the
  // next one.
  int64_t Memory::_free_run_count(int64_t address, int64_t end_address)
  {
    uint8_t hex_byte = 0;
    int64_t count = 0;

    this->_hexmap_io.seekg(this->_pointer_hexmap_address(address), std::ios::beg);

    while (address + (count * CHUNK_SIZE) < end_address) {
      this->_hexmap_io.read_std_ubyte(hex_byte);

      if (!this->_hexmap_io.good()) {
	this->_hexmap_io.clear();
	throw MemoryException("hexmap ends inside the free run at " + std::to_string(address));
      }

      // An allocated chunk is not part of this run and must not be counted:
      // counting it is how a free entry comes to cover a live record.
      if ((hex_byte & (uint8_t)128) == 128) break;

      count++;

      if ((hex_byte & (uint8_t)64) == 64) break;   // standalone: the run ends here
    }

    return count;
  }

  void Memory::_free_hexmap(int64_t address, int64_t count)
  {
    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_hexmap_io.good()) this->_hexmap_io.clear();
    this->_hexmap_io.seekp(address, std::ios::beg);
    this->_hexmap_io.fill_n(count - 1, FREE_CHUNK);
    this->_hexmap_io.write_std_ubyte(FREE_STANDALONE_CHUNK);
  }

  void Memory::_allocate_page(hashkey_ptr hash_key, int64_t address)
  {
    this->_full_hexmap(this->_pointer_hexmap_address(address), PAGEFILE_CONTROL_COUNT);
    this->_free_hexmap(this->_pointer_hexmap_address(address) + PAGEFILE_CONTROL_COUNT, this->_hbpp - PAGEFILE_CONTROL_COUNT);
    
    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_data_io.good()) this->_data_io.clear();
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

    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_data_io.good()) this->_data_io.clear();
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

	// SIZE, NOT THE ENTRY'S SIZE. A free entry measures the whole span it
	// covers -- data chunks and the control block -- because that is what
	// _free wrote and what the comparison above needs. A Pointer handed to
	// a caller measures the DATA, which is what the other two returns here
	// answer and what _pointer_actual_size expects to add the control block
	// back onto.
	//
	// Returning the entry as it stood gave back a Pointer whose size was
	// already CONTROL_SIZE too large, so the record's hexmap run was
	// written CONTROL_COUNT chunks too long and ran over whatever followed
	// it. It was nearly unreachable while startup registered at most one
	// free run per page: an exact match needs the hole to be the size of
	// the record replacing it, and a page has to be walked properly before
	// its holes are known.
	Pointer pointer(iter->second.hash_key, iter->second.address, size);
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
    
    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_hexmap_io.good()) this->_hexmap_io.clear();
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
    
    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_hexmap_io.good()) this->_hexmap_io.clear();
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

    if (full_count * CHUNK_SIZE == this->_page_size - PAGEFILE_CONTROL_SIZE) {

      int64_t page_address = begin * CHUNK_SIZE - PAGEFILE_CONTROL_SIZE;
      this->_allocate_page(FREE_HASHKEY, page_address);

      // REKEYED IN THE LIST, NOT LISTED TWICE. The page is __FREE__ on disk
      // now, and _allocate_new_page finds returnable pages by looking the
      // page list up under FREE_HASHKEY -- so the entry has to move. This
      // used to insert a SECOND entry under the table's key and keep the
      // old one: the returned page was invisible to the allocator until a
      // restart rebuilt the list from disk, and every scan of the table
      // walked the freed page twice under a key it no longer carries.
      //
      // The erased entry's key is not freed here: page list keys are heap
      // copies almost everywhere, but _free_key inserts the static
      // FREE_HASHKEY itself, and twenty bytes is the wrong price for
      // guessing which one this was.
      auto page_iter = this->_page_list.equal_range(pointer.hash_key);
      for (auto iter = page_iter.first; iter != page_iter.second; iter++) {
	if (iter->second == page_address) {
	  this->_page_list.erase(iter);
	  break;
	}
      }

      auto free_key = new hashkey_t;
      std::memcpy(free_key, FREE_HASHKEY, HASHKEY_SIZE);
      this->_page_list.insert({free_key, page_address});

    } else {
      
      Pointer tmp_pointer(pointer.hash_key, begin * CHUNK_SIZE, full_count * CHUNK_SIZE);
      this->_free_list_insert(pointer.hash_key, tmp_pointer);
    }
  }

  // Every row of one table that is deleted and settled: committed as DELETED,
  // with no live transaction holding it. The walk is the one _cursor makes over
  // the hexmap, without the isolation rules -- those exist to hide dead rows,
  // and this is the one caller that is looking for them.
  void Memory::_dead_pointers(hashkey_ptr hash_key, std::list<Pointer>& dead)
  {
    uint8_t hex_byte = 0;

    // Copied under the lock before it is walked, for the reason spelled out in
    // _cursor: another thread allocating a page rotates this multimap's tree
    // while the iterator is following it. Here the walk feeds TRUNCATE, so
    // wandering into another key's pages would free rows belonging to a table
    // nobody asked about.
    std::vector<std::pair<hashkey_ptr, int64_t> > pages;
    {
      std::lock_guard<std::mutex> page_list_lock(this->_page_list_access);
      auto pair_iter = this->_page_list.equal_range(hash_key);
      for (auto iter = pair_iter.first; iter != pair_iter.second; iter++)
	pages.push_back(std::make_pair(iter->first, iter->second));
    }

    for (size_t page_n = 0; page_n < pages.size(); page_n++) {

      hashkey_ptr page_hashkey = pages[page_n].first;
      int64_t page_address = pages[page_n].second;
      int64_t i = PAGEFILE_CONTROL_COUNT;

      while (i < this->_hbpp) {

	int64_t hexmap_address = this->_pointer_hexmap_address(page_address) + i;
	int64_t pointer_address = hexmap_address * CHUNK_SIZE;

	this->_hexmap_io.seekg(hexmap_address, std::ios::beg);

	this->_hexmap_io.read_std_ubyte(hex_byte);
	RowState online_state = (RowState)(hex_byte & (uint8_t)12);
	RowLock  online_lock  = (RowLock)(hex_byte & (uint8_t)3);

	this->_hexmap_io.read_std_ubyte(hex_byte);
	RowState offline_state = (RowState)(hex_byte & (uint8_t)12);

	this->_hexmap_io.seekg(CONTROL_COUNT - 2, std::ios::cur);

	i += CONTROL_COUNT;
	for (int64_t pointer_size = 0; i < this->_hbpp; pointer_size += CHUNK_SIZE) {

	  this->_hexmap_io.read_std_ubyte(hex_byte);
	  i++;

	  if ( hex_byte != CONTROL_STANDALONE_CHUNK && (hex_byte & (uint8_t)64) == 64 ) { // last chunk is standalone
	    if ( hex_byte != CONTROL_CHUNK && (hex_byte & (uint8_t)128) == 128 ) {        // last chunk is data

	      pointer_size += _size_remainder(hex_byte);

	      // Settled means the death is committed and nobody is mid-flight on
	      // the row. A row a live transaction still holds is left where it is.
	      // UPDATED counts as dead here because only a SUPERSEDED version
	      // ever carries it -- _commit_pointer stamps the new version
	      // INSERTED and the old one UPDATED -- and skipping them left a
	      // much-updated row's every old version behind forever.
	      if ((offline_state == RowState::DELETED || offline_state == RowState::UPDATED)
		  && online_state == RowState::NONE
		  && online_lock == RowLock::NONE) {
		dead.push_back(Pointer(page_hashkey, pointer_address, pointer_size));
	      }
	    }
	    break;
	  }
	}
      }
    }
  }

  // What DELETE leaves behind. A deleted row keeps its chunks so the store can
  // still be read at an earlier point in time, and nothing reclaims them --
  // _rollback_pointer says as much. This is the operation that gives up that
  // history in exchange for the space: it frees every settled dead row of one
  // table, and _free coalesces each one with the free space beside it, so a page
  // that ends up empty goes back to the allocator to be used by any table.
  size_t Memory::truncate(hashkey_ptr hash_key)
  {
    if (this->_watcher) {
      this->dba_watch("Truncate: (hash_key)");
    }

    Streams streams(this);

    // Collected in full before anything is freed: _free rewrites the hexmap and
    // moves entries between the page and free lists, which is exactly what the
    // walk above is reading.
    std::list<Pointer> dead;
    this->_dead_pointers(hash_key, dead);

    for (const Pointer& pointer : dead) {
      this->_free(pointer);
    }

    this->_hexmap_io.flush();
    this->_data_io.flush();

    return dead.size();
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

    // Through the read accessor: index lookups resolve pointers through this
    // while a shared-guard cursor may be doing the same on another thread.
    binarystream& hin = this->_hex_in();
    if (!hin.good()) hin.clear();

    hin.seekg(this->_pointer_hexmap_data_address(address), std::ios::beg);

    do {
      hin.read_std_ubyte(hex_byte);

      // Reading past the end of the hexmap leaves the stream in a failed state
      // and every further read a no-op, so without this the loop spins forever
      // on a truncated or empty store rather than reporting the problem.
      if (!hin.good()) {
	hin.clear();
	throw MemoryException("hexmap ends inside the chunk at " + std::to_string(address));
      }

      // Every chunk of a record is a DATA chunk -- the high bit is set on all
      // of them, standalone or not. A byte without it is free space, and free
      // space is where a torn chain link points: without this the walk reads
      // the hexmap one free byte at a time to its end (a FREE_STANDALONE byte
      // even "ends" it with a bogus pointer), which measured as the vacuum
      // spinning forever at address 0 with the whole server queued behind its
      // exclusive guard.
      if ( (hex_byte & (uint8_t)128) != 128)
	throw MemoryException("the chunk at " + std::to_string(address) + " is not an allocated record -- a torn link");

      if ( (hex_byte & (uint8_t)64) == 64) { // last chunk is standalone
	return Pointer(hash_key, address, size + _size_remainder(hex_byte));
      }
      size += CHUNK_SIZE;
    } while (true);
  }

  Pointer Memory::_pointer(hashkey_ptr hash_key, const Long& address)
  {
    return this->_pointer(hash_key, address.value());
  }

  // Whether ADDRESS names an allocated record: its first hexmap byte carries
  // the DATA bit. A chain walk asks this before following a link, because a
  // torn chain edit -- one the process died inside of -- reads as whatever
  // bytes landed: NULL ended chains before (see the walks in btreeindex.hpp),
  // and a ZERO arrives here, a "valid" address that in practice points at
  // free space. One such link sent the vacuum's walk spinning at address 0
  // with the server queued behind its exclusive guard until it was killed.
  bool Memory::_chain_resolves(int64_t address)
  {
    if (address < 0) return false;

    binarystream& hin = this->_hex_in();
    if (!hin.good()) hin.clear();
    hin.seekg(this->_pointer_hexmap_data_address(address), std::ios::beg);

    uint8_t hex_byte = 0;
    hin.read_std_ubyte(hex_byte);
    if (!hin.good()) { hin.clear(); return false; }

    return (hex_byte & (uint8_t)128) == 128;
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
	return Pointer(hash_key, address, size + _size_remainder(hex_byte));
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
        return  Pointer(hash_key, address, size + _size_remainder(hex_byte));
      }
      size += CHUNK_SIZE;
    } while (true);
  }

  void Memory::_sync()
  {
    this->_hexmap_io.flush();
    this->_data_io.flush();

    // Data before hexmap, every time. The hexmap is what says a record is
    // there and what state it is in; the data is the record itself. A hexmap
    // that reaches the disk first describes a record that may not have, and
    // that is the one ordering the store cannot recover from.
    this->_data_io.sync_to_disk();
    this->_hexmap_io.sync_to_disk();
  }

  bool Memory::_visible(Pointer& pointer, Streams* streams)
  {
    uint8_t hex_byte = 0;

    binarystream& hin = this->_hex_in();
    if (!hin.good()) hin.clear();

    hin.seekg(this->_pointer_hexmap_address(pointer.address), std::ios::beg);

    hin.read_std_ubyte(hex_byte);
    RowState online_state = (RowState)(hex_byte & (uint8_t)12);
    RowLock  online_lock  = (RowLock)(hex_byte & (uint8_t)3);

    hin.read_std_ubyte(hex_byte);
    RowState offline_state = (RowState)(hex_byte & (uint8_t)12);

    Control control;
    bool is_visible = false;
    bool settled = false;

    switch ((this->_initialized) ? Memory::transaction.isolation_level() : IsolationLevel::READ_UNCOMMITTED) {

    case IsolationLevel::READ_UNCOMMITTED:
      if ( (offline_state == RowState::NONE     && online_state == RowState::INSERTED) ||
	   (offline_state == RowState::INSERTED && online_state == RowState::NONE    ) ) {
	is_visible = true;
      }
      break;

    case IsolationLevel::READ_COMMITTED:
      // Every address that holds anything at all, because a version that has
      // been superseded may still be the one this read is entitled to. The
      // hexmap cannot answer that -- only the times in the control block can.
      if (offline_state != RowState::NONE || online_state != RowState::NONE) {
	is_visible = this->_read_committed(pointer, control);
      }
      break;

    case IsolationLevel::REPEATABLE_READ:
    case IsolationLevel::SERIALIZABLE: // handled in transaction class as semaphore
      if ((offline_state == RowState::NONE && online_state != RowState::NONE) || offline_state == RowState::INSERTED) {

	if (online_lock == RowLock::NONE) {
	  // A stamp is a stage too -- see _transaction_push. Begun BEFORE the
	  // control is written, or the stamp would carry the retired id.
	  if (this->_initialized && Memory::transaction.pointer.hash_key == nullptr)
	    this->begin_transaction();

	  this->_load_control(pointer, control);

	  control.online_state = RowState::NONE;
	  control.online_lock = RowLock::SHARED;
	  control.online_time = Memory::version_time();
	  control.transaction_id = Memory::transaction.id;
	  control.query_id = Memory::transaction.query_id;
	  this->_dump_control(pointer, control);
	  this->_transaction_push(pointer);
	  is_visible = true;
	} else {
	  is_visible = this->_check_lock(RowLock::EXCLUSIVE | RowLock::SHARED, pointer, control, streams);
	}
      }
      break;

    case IsolationLevel::SNAPSHOT:
      if ( (offline_state == RowState::NONE && online_state != RowState::NONE)
	   || offline_state == RowState::INSERTED || offline_state == RowState::DELETED) {

	do {
	  this->_load_control(pointer, control);

	  if (control.transaction_id > 0 && Memory::transaction.id == control.transaction_id) { // own lock
	    is_visible = (control.online_state == RowState::INSERTED);
	    break;
	  }

	  switch (control.offline_state) {
	  case RowState::NONE:
	    settled = true;
	    break;

	  case RowState::INSERTED:
	  case RowState::UPDATED:
	    if (control.create_time <= Memory::transaction.init_time) {
	      is_visible = true;
	      settled = true;
	    } else if (control.reference_address > -1) {
	      pointer = this->_pointer(pointer.hash_key, control.reference_address);
	    } else {
	      settled = true;
	    }
	    break;

	  case RowState::DELETED:
	    if (control.modify_time > Memory::transaction.init_time
		&& control.create_time != (time_t)0
		&& control.create_time <= Memory::transaction.init_time) {
	      is_visible = true;
	      settled = true;
	    } else if (control.reference_address > -1) {
	      pointer = this->_pointer(pointer.hash_key, control.reference_address);
	    } else {
	      settled = true;
	    }
	    break;

	  default:
	    throw MemoryException("invalid row state");
	  }
	} while (!settled);
      }
      break;

    default:
      throw MemoryException("invalid isolation level");
    }

    return is_visible;
  }

  // Only rows that reach the READ COMMITTED branches of _cursor and _visible get
  // here, and those have already filtered on the hexmap: a row is here because
  // it has a committed version (offline INSERTED) or because it has none and
  // something is staged on it (offline NONE, online not NONE). Rows whose
  // committed state is UPDATED -- superseded -- or DELETED never arrive.
  //
  // So the question reduces to: is there a committed version at this address,
  // and has this transaction itself done something that changes what it should
  // see? Nothing else matters, and in particular the lock does not: another
  // transaction's staged work is not this one's to see, whether it is still
  // holding it or not.
  bool Memory::_read_committed(const Pointer& pointer, Control& control)
  {
    this->_load_control(pointer, control);

    // What this transaction has staged itself, it is entitled to see the effect
    // of. The rules are the ones _check_lock has always applied to an own lock.
    if (control.online_lock != RowLock::NONE &&
	control.transaction_id == Memory::transaction.id) {

      // An insert THIS query made is hidden from it, so that an update cursor
      // does not walk over its own output and go round for ever.
      if (control.online_state == RowState::INSERTED &&
	  Memory::transaction.query_time > 0 &&
	  Memory::transaction.query_id == control.query_id)
	return false;

      // Superseded or removed by us: what is at this address is the version we
      // have already replaced.
      if (control.online_state == RowState::UPDATED ||
	  control.online_state == RowState::DELETED)
	return false;

      // Our own insert from earlier in this transaction.
      if (control.online_state == RowState::INSERTED)
	return true;
    }

    // Anybody else's staged work is invisible, and what is left is whatever was
    // committed here -- as of when this read began, not as of this instant. A
    // row another transaction is updating or deleting still has its committed
    // version at this address, and that is the one to show; showing it needs no
    // wait.
    return this->_alive_at(control, this->_snapshot_time());
  }

  bool Memory::_alive_at(const Control& control, time_t at)
  {
    // Never committed: there is nothing at this address anybody else may see.
    if (control.offline_state == RowState::NONE) return false;

    // Flagged dead at rollback without ever being born: create_time 0 with a
    // death state is _rollback_pointer's never-committed shape from before it
    // stamped the birth too. Nobody may see it at any time.
    if (control.create_time == (time_t)0
	&& (control.offline_state == RowState::DELETED || control.offline_state == RowState::UPDATED))
      return false;

    // Born after this read began. A committed row with no create_time at all is
    // one an older build wrote, and the honest reading of that is "long ago".
    if (control.create_time != (time_t)0 && control.create_time > at) return false;

    // Died at or before it -- but modify_time only MEANS died when the state
    // says so. _offline_update rewrites a row in place and stamps modify_time on
    // it while it is still perfectly current: that is how the index catalogue
    // records its root address, and how a B-tree key records its value chain.
    // Reading that stamp as a death made every index invisible to the reader
    // that came after the first write to it, so a reopened store looked empty
    // and built itself a fresh, blank index.
    //
    // An update or a delete is the only thing that retires a version, and both
    // set the state alongside the stamp.
    if ((control.offline_state == RowState::UPDATED ||
	 control.offline_state == RowState::DELETED) &&
	control.modify_time != (time_t)0 &&
	control.modify_time <= at)
      return false;

    return true;
  }

  time_t Memory::_snapshot_time()
  {
    return (Memory::transaction.query_time != (time_t)0)
      ? Memory::transaction.query_time
      : Memory::version_time();
  }

  bool Memory::_check_lock(RowLock lock, const Pointer& pointer, Control& control, Streams* streams)
  {
    if (this->_watcher) {
      this->dba_watch("Check Lock: (address: " + std::to_string(pointer.address) + ")");
    }
    
    bool do_read = true;
    int waited_ms = 0;
    do {
      this->_load_control(pointer, control);

      if (streams) streams->unlock();
	  
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
	    // A LOCK WHOSE TRANSACTION IS DEAD IS DEBRIS, NOT CONTENTION. A
	    // crashed client or an abandoned pool slot leaves its staged lock
	    // on disk with nothing left to commit or roll it back; only a
	    // restart's recovery would sweep it, and until then every waiter
	    // burned its whole timeout on a corpse -- the wedged row that
	    // stranded a machine for run after run. So recovery is executed
	    // lazily, here, on contact: roll the one pointer back exactly as
	    // startup would and look at the row again.
	    if (this->_initialized && !Memory::transaction_live(control.transaction_id)) {
	      if (streams) streams->lock();
	      if (this->_watcher) {
		this->dba_watch("Break Stale Lock: (address: " + std::to_string(pointer.address) + ")");
	      }
	      this->_rollback_pointer(pointer, true);
	      continue;
	    }

	    // Two sessions can each hold a lock the other is waiting for. Give up
	    // rather than spin forever: the caller rolls back and retries, which
	    // is what breaks the cycle.
	    if (Memory::lock_wait_timeout_ms > 0 && waited_ms >= Memory::lock_wait_timeout_ms) {
	      if (streams) streams->lock();
	      throw MemoryException("lock wait timeout");
	    }

	    std::this_thread::sleep_for(std::chrono::milliseconds(10));
	    waited_ms += 10;

	    if (streams) streams->lock();
	  }

	}

      } else {
	break; // no lock available
      }

    } while (true); // wait until lock is freed

    if (streams) streams->lock();

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

  void Memory::_cursor(hashkey_ptr hash_key, Streams* streams, std::function<bool (const Pointer&)> callback)
  {
    thread_local static uint8_t cursor_id = 0;
    if (++cursor_id >= MAX_CURSOR_COUNT) {
      throw MemoryException("maximum cursor count");
    }

    // Where this scan's view of the store is fixed. Nested cursors -- and the
    // retry below, which calls back into here -- belong to the statement that is
    // already running rather than starting one of their own.
    Statement statement(this);

    uint8_t hex_byte = 0;
    int64_t hexmap_address = -1;
    int64_t pointer_address = -1;
    RowState online_state = RowState::NONE;
    RowState offline_state = RowState::NONE;
    RowLock online_lock = RowLock::NONE;
    Control control;

    // THE PAGE LIST IS COPIED BEFORE IT IS WALKED, and that is a correctness
    // fix rather than a tidying.
    //
    // `_page_list' is a std::multimap that _allocate_page and _free_key modify
    // under _page_list_access. This walk took no lock at all. Inserting into a
    // multimap does not invalidate an iterator, which is what makes the mistake
    // easy to make -- but it does rotate the red-black tree, and an iterator
    // being incremented follows the very parent and child pointers a rotation
    // is rewriting. The walk then lands in a subtree belonging to some other
    // hash key, and the scan reads rows that are not its table's at all.
    //
    // That is `readers_do_not_queue_behind_staged_writes' failing about one run
    // in three: four sessions insert, each allocating pages while the others
    // scan, and a scan comes back holding somebody else's staged row.
    //
    // HELD ACROSS THE COPY AND NOT THE WALK. The callback below runs with the
    // streams handed back and is free to insert, which takes this same lock --
    // so holding it for the length of the scan would deadlock against the first
    // callback that allocates a page. A page allocated after this copy is not
    // scanned, which is what a statement's fixed view means anyway.
    // AND RE-COPIED UNTIL A PASS ADDS NOTHING. The callback below runs with
    // the streams handed back, so a writer is free to commit rows into a page
    // this table did not have when the copy above was taken -- and a shared
    // guard widens that window to the whole scan. So the walk repeats over
    // whatever pages are NEW since the last pass, until a pass finds none:
    // the fixed point. Bounded, because a runaway committer must not be able
    // to hold a scan open forever. Ported from the Cicili engine with the
    // walk-length ager that taught it.
    std::vector<int64_t> walked;
    bool walk_done = false;
    int rounds = 0;

    while (!walk_done && rounds < 16) {
      rounds++;

    std::vector<std::pair<hashkey_ptr, int64_t> > pages;
    {
      std::lock_guard<std::mutex> page_list_lock(this->_page_list_access);
      auto pair_iter = this->_page_list.equal_range(hash_key);
      for (auto iter = pair_iter.first; iter != pair_iter.second; iter++) {
	bool seen = false;
	for (size_t w = 0; w < walked.size(); w++)
	  if (walked[w] == iter->second) { seen = true; break; }
	if (!seen)
	  pages.push_back(std::make_pair(iter->first, iter->second));
      }
    }

    if (pages.empty()) { walk_done = true; break; }

    for (size_t page_n = 0; page_n < pages.size(); page_n++) {

      hashkey_ptr page_hashkey = pages[page_n].first;
      int64_t page_address = pages[page_n].second;
      int64_t i = PAGEFILE_CONTROL_COUNT;

      walked.push_back(page_address);

      binarystream& hin = this->_hex_in();
      if (!hin.good()) hin.clear();

      while (i < this->_hbpp) {

	hexmap_address = this->_pointer_hexmap_address(page_address) + i;
	pointer_address = hexmap_address * CHUNK_SIZE;

	hin.seekg(hexmap_address, std::ios::beg);

	hin.read_std_ubyte(hex_byte);
	online_state = (RowState)(hex_byte & (uint8_t)12);
	online_lock = (RowLock)(hex_byte & (uint8_t)3);

	hin.read_std_ubyte(hex_byte);
	offline_state = (RowState)(hex_byte & (uint8_t)12);

	hin.seekg(CONTROL_COUNT - 2, std::ios::cur);

	i += CONTROL_COUNT;
	for (int64_t pointer_size = 0 ; i < this->_hbpp; pointer_size += CHUNK_SIZE) {

	  hin.read_std_ubyte(hex_byte);
	  i++;
	  
	  if ( hex_byte != CONTROL_STANDALONE_CHUNK && (hex_byte & (uint8_t)64) == 64 ) { // last chunk is standalone
	    if ( hex_byte != CONTROL_CHUNK && (hex_byte & (uint8_t)128) == 128 ) { // last chunk is data

	      pointer_size += _size_remainder(hex_byte);
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
		// Every address that holds anything at all -- see _visible.
		if (offline_state != RowState::NONE || online_state != RowState::NONE) {
		  do_callback = this->_read_committed(pointer, control);
		}
		break;
		
	      case IsolationLevel::REPEATABLE_READ:
	      case IsolationLevel::SERIALIZABLE: // handled in transaction class as semaphore
		if ((offline_state == RowState::NONE && online_state != RowState::NONE) || offline_state == RowState::INSERTED) {		  
		  this->_load_control(pointer, control);
		  
		  if (online_lock == RowLock::NONE) {
		    control.online_state = RowState::NONE;
		    control.online_lock = RowLock::SHARED;
		    control.online_time = Memory::version_time();
		    control.transaction_id = Memory::transaction.id;    
		    control.query_id = Memory::transaction.query_id;
		    this->_dump_control(pointer, control);
		    this->_transaction_push(pointer);
		    do_callback = true;
		  } else {
		    do_callback = this->_check_lock(RowLock::EXCLUSIVE | RowLock::SHARED, pointer, control, streams);
		    if ( (!do_callback && control.transaction_id > 0 && control.transaction_id != Memory::transaction.id) ||
			 (do_callback && control.create_time > Memory::transaction.query_time)) {
		      // rollback_transaction_to() takes the same streams this scan
		      // is already holding, so they have to be handed back before
		      // the retry -- its own guard then finds nothing held and
		      // takes them properly.
		      if (streams) streams->unlock();

		      this->rollback_transaction_to(Memory::transaction.query_time);
		      std::this_thread::sleep_for(std::chrono::milliseconds(10));

		      if (streams) streams->lock();

		      cursor_id--;
		      this->_cursor(hash_key, streams, callback);
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
		      if (control.modify_time > Memory::transaction.init_time
			  && control.create_time != (time_t)0
			  && control.create_time <= Memory::transaction.init_time) {
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

		if (streams) streams->unlock();
		
		if ( !callback( pointer ) ) {
		  i = this->_hbpp;
		}

		if (streams) streams->lock();

	      }
	      
	    } // if data
	    break;
	  } // if standalone
	} // for
      } // while
    }// for pages
    }// while !walk_done -- the fixed point

    --cursor_id;
  }

  void Memory::_control_insert(BaseTable* object)
  {
    // A stage needs a transaction -- see _transaction_push.
    if (this->_initialized && Memory::transaction.pointer.hash_key == nullptr)
      this->begin_transaction();

    if (this->_watcher) {
      this->dba_watch("Control Insert: (address: " + std::to_string(object->pointer.address) + ")");
    }
    
    Control control;

    control.online_state = RowState::INSERTED;
    control.online_lock = RowLock::EXCLUSIVE;

    control.online_time = Memory::version_time();
    control.transaction_id = Memory::transaction.id;
    control.query_id = Memory::transaction.query_id;
    
    this->_dump_control(object->pointer, control);
  }
  
  void Memory::_control_update(BaseTable* old_object, BaseTable* new_object, Streams* streams)
  {
    // A stage needs a transaction -- see _transaction_push.
    if (this->_initialized && Memory::transaction.pointer.hash_key == nullptr)
      this->begin_transaction();

    if (this->_watcher) {
      this->dba_watch("Control Update: (address: " + std::to_string(old_object->pointer.address) + ")");
    }

    Control control;
    
    if (!this->_check_lock(RowLock::EXCLUSIVE, old_object->pointer, control, streams))
      return;

    control.online_state = RowState::UPDATED;
    control.online_lock = RowLock::EXCLUSIVE;

    control.online_time = Memory::version_time();
    control.transaction_id = Memory::transaction.id;
    control.query_id = Memory::transaction.query_id;
    
    this->_dump_control(old_object->pointer, control);
    
    control = Control();
    
    control.online_state = RowState::INSERTED;
    control.online_lock = RowLock::EXCLUSIVE;
    
    control.online_time = Memory::version_time();
    control.transaction_id = Memory::transaction.id;
    control.query_id = Memory::transaction.query_id;
    control.reference_address = old_object->pointer.address;
    
    this->_dump_control(new_object->pointer, control);
  }
  
  void Memory::_control_delete(BaseTable* object, Streams* streams)
  {
    // A stage needs a transaction -- see _transaction_push.
    if (this->_initialized && Memory::transaction.pointer.hash_key == nullptr)
      this->begin_transaction();

    if (this->_watcher) {
      this->dba_watch("Control Delete: (address: " + std::to_string(object->pointer.address) + ")");
    }

    Control control;
 
    if (!this->_check_lock(RowLock::EXCLUSIVE, object->pointer, control, streams))
      return;
    
    control.online_state = RowState::DELETED;
    control.online_lock = RowLock::EXCLUSIVE;

    control.online_time = Memory::version_time();
    control.transaction_id = Memory::transaction.id;
    control.query_id = Memory::transaction.query_id;
    
    this->_dump_control(object->pointer, control);
  }
  
  void Memory::_transaction_push(const Pointer& pointer)
  {
    // A STAGE NEEDS A TRANSACTION. The layer above commits between
    // statements and does not always begin before the next one, so the next
    // statement's stages arrived under the RETIRED id of the transaction
    // that had just committed -- work that was alive by every intention but
    // dead by the registry, and the lazy stale-lock breaker rolled it back
    // from under the running statement: the machines rows lost their index
    // entries that way, one ghost per broken stage, and every ghost refused
    // a name forever. Beginning here is idempotent -- a begin on an open
    // transaction continues it -- so a stage that arrives with no
    // transaction open simply opens the next one.
    if (this->_initialized && Memory::transaction.pointer.hash_key == nullptr)
      this->begin_transaction();

    if (this->_watcher) {
      this->dba_watch("Transaction Push: (address: " + std::to_string(pointer.address) + ")");
    }

    // Same clock as query_time, because rollback_transaction_to compares the two.
    Memory::transaction.context.emplace_back(Memory::version_time(), pointer);
  }

  void Memory::_commit_pointer(const Pointer& pointer, time_t commit_time)
  {
    if (this->_watcher) {
      this->dba_watch("Commit Pointer: (address: " + std::to_string(pointer.address) + ")");
    }

    Control control;
    this->_load_control(pointer, control);

    // ONLY A STAGE THIS TRANSACTION STILL OWNS IS ITS TO FLIP. A writer that
    // does not conflict with a shared stamp can stage over it, and the
    // displaced transaction's commit arriving later would flip the
    // DISPLACER'S uncommitted work with its own timestamp. Startup recovery
    // stands outside the rule: it executes crashed transactions' recorded
    // intentions, so foreign ids are exactly its job.
    if (this->_initialized && control.transaction_id != Memory::transaction.id)
      return;
  
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

  void Memory::_rollback_pointer(const Pointer& pointer, bool as_recovery)
  {
    if (this->_watcher) {
      this->dba_watch("Rollback Pointer: (address: " + std::to_string(pointer.address) + ")");
    }

    Control control;
    this->_load_control(pointer, control);

    // The same ownership rule as _commit_pointer -- a partial rollback
    // 'restoring' a row whose stage now belongs to another transaction
    // erased that transaction's staged delete and resurrected the row.
    // Recovery stands outside the rule as it always has: as_recovery is
    // _check_lock breaking a dead transaction's lock, whose whole point is
    // rolling back a stage that is nobody's.
    if (!as_recovery && this->_initialized && control.transaction_id != Memory::transaction.id)
      return;

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
	control.modify_time = Memory::version_time();
	// Born and died at the same instant, so no snapshot anywhere can see
	// it. Without the birth stamp, a reader whose statement began BEFORE
	// this rollback read "died after my snapshot" on a row that was never
	// alive -- the dirty read readers_do_not_queue_behind_staged_writes
	// caught about one run in three.
	control.create_time = control.modify_time;
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

    // A failed read leaves the stream poisoned and every write after it a
    // silent no-op -- which is how a record can end up allocated and never
    // written. Writes clear first, exactly as the read accessors do.
    if (!this->_data_io.good()) this->_data_io.clear();
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

    // A FRESH ID PER FRESH BEGIN, not per thread and not per begin. The
    // constructor's per-thread id meant every transaction a pooled
    // connection ever ran shared one id, so "is the transaction that
    // stamped this lock still running" had no answer -- the id outlived
    // every one of them. But a begin on a thread whose transaction is
    // STILL OPEN continues that transaction: the server begins once per
    // request while a turn's transaction spans many, and per-thread ids
    // made that harmless -- one commit flipped the whole merged context.
    // Handing such a begin a fresh id orphaned every stage the turn had
    // already made (the commit's ownership guard skipped them all), and
    // a fresh store grew ghost machine rows and torn index chains within
    // a handful of runs. So the id changes only when no transaction is
    // open -- commit and rollback null the record pointer, which is the
    // test -- and an id then names exactly one transaction, which is what
    // the live registry and the stale-lock breaker in _check_lock key on.
    // The serial makes two begins distinct even within one clock second;
    // the hashed base keeps ids from different runs from colliding in a
    // store that outlives them.
    if (Memory::transaction.pointer.hash_key == nullptr) {
      static const size_t id_base = Utility::generate_id();
      static std::atomic<size_t> id_serial{0};
      Memory::transaction.id = id_base + (++id_serial);
      if (Memory::transaction.id == 0) Memory::transaction.id = ++id_serial;
      Memory::_transaction_register(Memory::transaction.id);
    }

    // Allocating the transaction record touches the hexmap and the data file
    // exactly as commit and rollback do, so it needs the same two mutexes.
    // Without them concurrent sessions raced here and lost each other's pages.
    Streams streams(this);

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

    // A transaction that wrote nothing has nothing to make durable: no
    // control block moved, so there is no intention to record and no sync to
    // pay. Its record stays open on disk and _initialize treats an intention
    // that licenses nothing as the no-op rollback it is, then frees it with
    // the other spent records. This is not a nicety: a polling client COMMITS
    // ITS READS, and the full dance below is three syncs of two files per
    // poll.
    // The record itself is freed here, exactly as rollback frees its own --
    // without it every poll left a 64-byte record behind that only a restart
    // would sweep. No sync: a crash resurrects an open record that licenses
    // nothing, and startup already treats that as the no-op it is.
    if (Memory::transaction.context.empty()) {
      {
	Streams streams(this);
	if (Memory::transaction.pointer.hash_key != nullptr)
	  this->_free(Memory::transaction.pointer);
	Memory::transaction.pointer = Pointer();
      }
      Memory::transaction_retire(Memory::transaction.id);
      Memory::transaction.reset();
      return;
    }

    // Scoped, because Transaction::reset() below must not run under it: reset
    // can wait on the SERIALIZABLE semaphore, and the thread that would release
    // that semaphore has to get through its own commit to do so -- which needs
    // these. Streams then semaphore, in that order, and never the other way.
    {
    Streams streams(this);

    // ONE stamp for the whole transaction, and every version it touches gets it.
    // That is what makes an update change over cleanly: the old version's
    // modify_time and the new one's create_time are the same instant, so a
    // reader at any time sees exactly one of them.
    time_t commit_time = Memory::version_time();

    // Three writes in an order that has to survive the power going out.
    //
    // A row is updated by writing a whole new version elsewhere and then
    // flipping the control block that adopts it, which is shadow paging and
    // needs no log to recover -- but only if the writes land in the order they
    // were made. flush() hands them to the operating system, which is entitled
    // to reorder them, so without a sync the guarantee is a wish.
    //
    // First the intention: this transaction is committing at commit_time, on
    // disk before any control block moves. _initialize reads exactly this at
    // startup to decide whether an uncommitted looking record should be
    // committed or rolled back, so a crash between here and the loop below
    // still resolves the same way.
    this->_write_transaction(Memory::transaction.id, commit_time);
    this->_sync();

    // Then the control blocks it licenses.
    for (const auto& pair : Memory::transaction.context) {
      this->_commit_pointer(pair.second, commit_time);
    }
    Memory::transaction.context.clear();
    this->_sync();

    // And only then is the intention retired, because it is what a crash in the
    // middle of the loop would have been recovered from.
    this->_write_transaction(Memory::transaction.id, (time_t)0);
    this->_sync();
    }

    // THE RECORD POINTER IS SPENT WITH THE TRANSACTION. Only the read-only
    // path nulled it, so after a writing commit the next begin CONTINUED a
    // committed-out transaction -- its stages carried a retired id, and the
    // lazy breaker ate live work (the wire never showed it because the
    // request layer's rollback hygiene nulls the pointer between requests;
    // the embedded arrangement has no such layer and went twelve reds in
    // twelve runs). The record chunk itself stays for the startup sweep,
    // exactly as before.
    Memory::transaction.pointer = Pointer();

    // Retired from the live registry only now, with every lock already
    // cleared above: a waiter that still sees this id on a lock must find it
    // live, or it would break a lock whose commit is mid-flight.
    Memory::transaction_retire(Memory::transaction.id);

    // THE TRANSACTION IS OVER, so what belonged to it goes back to the
    // connection's defaults -- the isolation level above all, which is how a
    // SERIALIZABLE slot is given back. Without this a connection kept whatever
    // level one procedure asked for until it closed.
    Memory::transaction.reset();
  }
  
  void Memory::commit_transaction_until(time_t)
  {

  }

  void Memory::rollback_transaction()
  {
    if (this->_watcher) {
      this->dba_watch("Rollback Transaction: ()");
    }

    // Scoped for the same reason commit_transaction's is: reset() below can
    // wait on the SERIALIZABLE semaphore and must not be holding these while
    // it does.
    {
    Streams streams(this);

    auto e_iter = Memory::transaction.context.crend();
    for (auto iter = Memory::transaction.context.crbegin(); iter != e_iter; iter++) {
      this->_rollback_pointer(iter->second);
    }
    Memory::transaction.context.clear();

    // Freed once, and then forgotten -- because after this the pointer names
    // space the allocator has taken back, and a second rollback that still
    // believes in it hands the same chunks to the free list twice.
    //
    // Nothing stopped that happening. Zeytun rolls a request back in
    // RequestScope's destructor whenever the page did not commit, and
    // Transaction's own destructor rolls back again when the pooled worker
    // thread ends -- two rollbacks, one pointer. The free list then holds the
    // chunk twice, two unrelated allocations are handed the same address, and
    // whichever writes second wins. A BTreeKey landing on a BTreeValue is how
    // it showed: a key unpacks seven fields where a value wrote two, so the key
    // field reads past the record and comes back null, and the next insert dies
    // in Long::operator< walking into it.
    //
    // A default Pointer has no hash key, which is also what a transaction that
    // never allocated one looks like. Neither is something to free.
    if (Memory::transaction.pointer.hash_key != nullptr)
      this->_free(Memory::transaction.pointer);

    Memory::transaction.pointer = Pointer();

    this->_sync();
    }

    // A rolled back transaction is as over as a committed one -- and retired
    // the same way, after its locks are gone.
    Memory::transaction_retire(Memory::transaction.id);
    Memory::transaction.reset();
  }

  void Memory::rollback_transaction_to(time_t time)
  {
    if (this->_watcher) {
      this->dba_watch("Rollback Transaction to: (" + std::string(asctime(gmtime(&time))) + ")");
    }

    Streams streams(this);    

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
    // Under the pair, like everything else that seeks and reads the shared
    // streams. This walked them with nothing held -- an admin connection
    // asking for a page dump while any session wrote raced every seek
    // against every read, which is the exact interleaving Streams exists
    // to prevent.
    Streams streams(this);

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
	  stream.write_std_long(size + _size_remainder(hex_byte));
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
