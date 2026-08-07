
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
