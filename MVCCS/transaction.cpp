#include "transaction.h"
#include "memory.h"
#include "globals.h"
#include "utility.h"


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
    this->init_time = std::time(0);
    this->query_time = 0;
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
      Transaction::_serialize_cv.wait(serialize_lock, [] () { return Transaction::_serialize_counter == 0; });
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
