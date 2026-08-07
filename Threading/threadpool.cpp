#include "threadpool.hpp"
#include "threadingexception.hpp"
#include <iostream>


namespace Zigurat
{

  ThreadPool::ThreadPool()
    : _discharged(true)
  {

  }

  void ThreadPool::charge(size_t size)
  {
    if (!this->_discharged) throw ThreadingException("thread pool is charged");
    if (size == 0) throw ThreadingException("thread pool size must be positive");

    {
      std::unique_lock<std::mutex> lock(this->_queue_guard);
      this->_stopping = false;
      this->_queue.clear();
    }

    this->_size = size;
    this->_discharged = false;

    for (size_t i = 0; i < this->_size; i++)
      this->_pool.emplace_back(&ThreadPool::_handler, this);
  }

  void ThreadPool::_handler()
  {
    while (true) {
      Job job;

      {
	std::unique_lock<std::mutex> lock(this->_queue_guard);
	this->_semaphore.wait(lock, [this] { return this->_stopping || !this->_queue.empty(); });

	// Draining takes priority over stopping, so a graceful discharge still
	// runs everything that was already queued.
	if (this->_queue.empty()) return;

	job = this->_queue.front();
	this->_queue.pop_front();
      }

      // A job that throws must not take the worker, and with it the whole
      // process, down with it.
      try {
	job();
      } catch (const std::exception& e) {
	std::cerr << "[ThreadPool] job failed: " << e.what() << std::endl;
      } catch (...) {
	std::cerr << "[ThreadPool] job failed with a foreign exception" << std::endl;
      }
    }
  }

  size_t ThreadPool::size() const
  {
    return this->_size;
  }

  void ThreadPool::limit(size_t limit)
  {
    std::unique_lock<std::mutex> lock(this->_queue_guard);
    this->_limit = limit;
  }

  size_t ThreadPool::queued()
  {
    std::unique_lock<std::mutex> lock(this->_queue_guard);
    return this->_queue.size();
  }

  void ThreadPool::execute(Job job)
  {
    std::unique_lock<std::mutex> lock(this->_queue_guard);
    if (this->_stopping) throw ThreadingException("thread pool is discharging");

    // Refusing is a service. The queue used to be the only limit on how much
    // work the process would take on, and a deque accepts jobs until the
    // machine will not give it any more memory -- so a server whose workers
    // were all busy went on saying yes, and fell over rather than turning
    // anyone away.
    if (this->_limit > 0 && this->_queue.size() >= this->_limit)
      throw ThreadingException("thread pool queue is full");

    this->_queue.push_back(job);
    lock.unlock();
    this->_semaphore.notify_one();
  }

  // force discards whatever is still queued; either way the workers are joined
  // before the pool is torn down, because destroying a joinable std::thread
  // calls std::terminate.
  void ThreadPool::discharge(bool force)
  {
    if (this->_discharged) return;

    {
      std::unique_lock<std::mutex> lock(this->_queue_guard);
      this->_stopping = true;
      if (force) this->_queue.clear();
    }

    this->_semaphore.notify_all();

    for (size_t i = 0; i < this->_pool.size(); i++)
      if (this->_pool[i].joinable()) this->_pool[i].join();

    this->_pool.clear();
    this->_size = 0;
    this->_discharged = true;
  }

  ThreadPool::~ThreadPool()
  {
    if (!this->_discharged) this->discharge(true);
  }

}
