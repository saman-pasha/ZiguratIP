
#ifndef __TRANSACTION_HPP__
#define __TRANSACTION_HPP__


#include <cstdint>
#include "isolationlevel.hpp"
#include "pointer.hpp"
#include <ctime>
#include <mutex>
#include <condition_variable>
#include <list>

namespace Zigurat
{

  class Transaction
  {
  private:
    static size_t _serialize_counter;
    static std::mutex _serialize_mutex;
    static std::condition_variable _serialize_cv;
    // set_isolation_level() reads this before assigning, so it cannot start out
    // as whatever happened to be on the stack.
    IsolationLevel _isolation_level = IsolationLevel::READ_COMMITTED;
  public:
    Transaction();
    void initialize(Pointer);

    // Puts everything that belongs to ONE transaction back to its starting
    // state, keeping what belongs to the connection. Commit and rollback end a
    // transaction, so both call it.
    //
    // WHY IT HAS TO EXIST. A connection holds one thread and one Transaction
    // for its whole life, and initialize() ran once, when the connection was
    // made. So anything a transaction changed about itself outlived it: a
    // procedure carrying TRANSACTION ISOLATION LEVEL SERIALIZABLE left the
    // connection serializable for ever after, and -- because SERIALIZABLE is a
    // semaphore of one -- left it holding that slot after the commit. Every
    // other connection that asked for SERIALIZABLE then waited on a slot whose
    // owner had long since finished with it, which looks from the outside like
    // one client doing all the work and the rest hanging. init_time is the same
    // kind of leak with a quieter symptom: it is what SNAPSHOT compares against,
    // so a second transaction on one connection went on seeing the first one's
    // view of the store.
    void reset();

    size_t id;
    time_t init_time;
    Pointer pointer;
    int64_t query_id;
    time_t query_time;
    bool auto_commit;
    std::list< std::pair<std::time_t, Pointer> > context;
    IsolationLevel isolation_level();
    void set_isolation_level(IsolationLevel);
    virtual ~Transaction();
  };
  
}

#endif // __TRANSACTION_HPP__
