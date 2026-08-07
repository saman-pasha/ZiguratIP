
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
    size_t _limit = 0;        // guarded by _queue_guard; 0 is no limit

  public:
    ThreadPool();
    void charge(size_t = Thread::hardware_concurrency());
    size_t size() const;

    // How many jobs may wait for a worker. Beyond this execute() refuses
    // rather than queueing, so a caller that cannot be served is told so while
    // it is still cheap to say. Without it the queue is the only thing
    // deciding how much work the process accepts, and a deque will hold as
    // many as memory allows -- which is the shape of every accept loop that
    // falls over under load rather than shedding it.
    void limit(size_t);
    size_t queued();

    void execute(Job);
    void discharge(bool);
    virtual ~ThreadPool();
  };

}

#endif // __THREADPOOL_HPP__
