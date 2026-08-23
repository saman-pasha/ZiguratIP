#include "transaction.hpp"
#include "memory.hpp"
#include "memoryexception.hpp"
#include "globals.hpp"
#include "utility.hpp"
#include <chrono>


namespace Zigurat
{

  size_t Transaction::_serialize_counter = 0;
  std::mutex Transaction::_serialize_mutex;
  std::condition_variable Transaction::_serialize_cv;

  Transaction::Transaction()
  {
    this->id = Utility::generate_id();

    if (Globals::memory())
      Globals::memory()->begin_transaction();
  }
  
  void Transaction::initialize(Pointer pointer)
  {
    this->pointer = pointer;
    this->reset();
  }

  void Transaction::reset()
  {
    // The version clock, not the wall clock: this is compared against the
    // create_time and modify_time of row versions, and a second's resolution
    // cannot tell two of those apart.
    this->init_time = Memory::version_time();
    this->query_time = 0;
    this->query_id = 0;

    // The connection's settings, not the last transaction's. Restoring the
    // isolation level here is also what gives back a SERIALIZABLE slot -- see
    // set_isolation_level, and the note in the header.
    this->auto_commit = Globals::default_autocommit_mode();
    this->set_isolation_level(Globals::default_isolation_level());
  }
  
  IsolationLevel Transaction::isolation_level()
  {
    return this->_isolation_level;
  }

  // SERIALIZABLE admits one transaction at a time. Both the counter and the
  // wait have to happen under the one mutex, and the slot has to be taken after
  // the wait: incrementing first meant two transactions could never both get
  // past the predicate, so everything past the first deadlocked.
  void Transaction::set_isolation_level(IsolationLevel iso_lvl)
  {
    if (this->_isolation_level == iso_lvl) return;

    std::unique_lock<std::mutex> serialize_lock(Transaction::_serialize_mutex);

    const bool was_serializable = (this->_isolation_level == IsolationLevel::SERIALIZABLE);
    if (was_serializable && Transaction::_serialize_counter > 0)
      Transaction::_serialize_counter--;

    if (iso_lvl == IsolationLevel::SERIALIZABLE) {
      // BOUNDED, for the same reason row locks are. The slot is given back when
      // its transaction commits or rolls back -- but a client is entitled to
      // open a transaction, take it, and then say nothing for an hour, and an
      // unbounded wait here made that one client's silence into every other
      // connection's hang, with nothing in the log to say why. Giving up says
      // so, and the caller can retry or carry on at a weaker level.
      const int timeout_ms = Memory::lock_wait_timeout_ms;
      const bool got = (timeout_ms > 0)
	? Transaction::_serialize_cv.wait_for(serialize_lock, std::chrono::milliseconds(timeout_ms),
					      [] () { return Transaction::_serialize_counter == 0; })
	: (Transaction::_serialize_cv.wait(serialize_lock,
					   [] () { return Transaction::_serialize_counter == 0; }), true);

      if (!got) {
	// Nothing has been taken and nothing changed, but the slot this
	// transaction used to hold was given up on the way in, so whoever is
	// waiting for it has to be told.
	serialize_lock.unlock();
	if (was_serializable) Transaction::_serialize_cv.notify_one();
	throw MemoryException("serializable wait timeout");
      }

      Transaction::_serialize_counter++;
    }

    this->_isolation_level = iso_lvl;

    serialize_lock.unlock();
    if (was_serializable) Transaction::_serialize_cv.notify_one();
  }

  // Runs when a thread ends, which for the thread_local instance can be during
  // process teardown. Anything that throws here terminates the process, so the
  // implicit rollback is best effort.
  Transaction::~Transaction()
  {
    try {
      if (Globals::memory())
	Globals::memory()->rollback_transaction();
    } catch (...) {
      // The store is already gone or unusable; nothing left to roll back to.
    }

    // The rollback above retires the id from the live registry; when there was
    // no store to roll back against, retire it here so a lock this transaction
    // managed to stamp before things went wrong reads as breakable debris, not
    // as a live owner.
    try {
      Memory::transaction_retire_why(this->id, 14);
    } catch (...) {
      // Process teardown may have destroyed the registry already.
    }

    if (this->_isolation_level == IsolationLevel::SERIALIZABLE) {
      try {
	std::unique_lock<std::mutex> serialize_lock(Transaction::_serialize_mutex);
	if (Transaction::_serialize_counter > 0) Transaction::_serialize_counter--;
	serialize_lock.unlock();
	Transaction::_serialize_cv.notify_one();
      } catch (...) {
	// The serialisation primitives outlive every normal transaction; if they
	// are already destroyed there is no queue left to release.
      }
    }
  }

}
