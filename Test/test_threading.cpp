#include "ztest.hpp"
#include "threadpool.hpp"
#include <atomic>
#include <chrono>
#include <thread>

using namespace Zigurat;


ZTEST(Threading, pool_runs_every_job)
{
  ThreadPool pool;
  pool.charge(4);
  ZCHECK_EQ(pool.size(), (size_t)4);

  std::atomic<int> counter(0);
  for (int i = 0; i < 200; i++)
    pool.execute([&counter] () { counter++; });

  pool.discharge(false);   // graceful: drain the queue first
  ZCHECK_EQ(counter.load(), 200);
}

// discharge() used to destroy joinable threads, which calls std::terminate.
ZTEST(Threading, pool_discharges_without_terminating)
{
  ThreadPool pool;
  pool.charge(2);

  std::atomic<int> counter(0);
  for (int i = 0; i < 10; i++)
    pool.execute([&counter] () { counter++; });

  ZCHECK_NOTHROW(pool.discharge(true));
  ZCHECK_EQ(pool.size(), (size_t)0);
}

ZTEST(Threading, pool_can_be_recharged)
{
  ThreadPool pool;

  pool.charge(2);
  std::atomic<int> first(0);
  pool.execute([&first] () { first++; });
  pool.discharge(false);
  ZCHECK_EQ(first.load(), 1);

  pool.charge(3);
  ZCHECK_EQ(pool.size(), (size_t)3);
  std::atomic<int> second(0);
  for (int i = 0; i < 30; i++) pool.execute([&second] () { second++; });
  pool.discharge(false);
  ZCHECK_EQ(second.load(), 30);
}

ZTEST(Threading, charging_twice_is_rejected)
{
  ThreadPool pool;
  pool.charge(2);
  ZCHECK_THROWS(pool.charge(2));
  pool.discharge(true);
}

// A throwing job must be contained, not allowed to unwind out of the worker.
ZTEST(Threading, a_throwing_job_does_not_kill_the_pool)
{
  ThreadPool pool;
  pool.charge(2);

  std::atomic<int> survived(0);
  pool.execute([] () { throw std::runtime_error("deliberate"); });
  for (int i = 0; i < 20; i++)
    pool.execute([&survived] () { survived++; });

  pool.discharge(false);
  ZCHECK_EQ(survived.load(), 20);
}

ZTEST(Threading, destructor_discharges_a_live_pool)
{
  std::atomic<int> counter(0);
  {
    ThreadPool pool;
    pool.charge(2);
    for (int i = 0; i < 5; i++) pool.execute([&counter] () { counter++; });
    // Left to the destructor deliberately.
  }
  ZCHECK(counter.load() >= 0);   // the point is that we got here at all
}
