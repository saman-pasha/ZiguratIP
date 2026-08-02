
#ifndef __THREADPOOL_HPP__
#define __THREADPOOL_HPP__

#include "thread.hpp"
#include "job.hpp"
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>

namespace Zigurat
{

  class ThreadPool
  {
  protected:
    size_t _size = 0;
    std::vector<Thread> _pool;
    std::deque<Job> _queue;
    std::mutex _queue_guard;
    std::condition_variable _semaphore;
    void _handler();
    bool _discharged;
    bool _stopping = false;   // guarded by _queue_guard

  public:
    ThreadPool();
    void charge(size_t = Thread::hardware_concurrency());
    size_t size() const;
    void execute(Job);
    void discharge(bool);
    virtual ~ThreadPool();
  };

}

#endif // __THREADPOOL_HPP__
