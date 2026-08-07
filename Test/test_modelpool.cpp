#include "ztest.hpp"
#include "modelpool.hpp"
#include "zexception.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Zigurat;

namespace
{
  // Something expensive enough to be worth pooling, and countable so a test can
  // say how many times it was built.
  struct Weights
  {
    static std::atomic<int> built;
    static std::atomic<int> destroyed;
    int value;
    explicit Weights(int v) : value(v) { built++; }
    ~Weights() { destroyed++; }
  };

  std::atomic<int> Weights::built(0);
  std::atomic<int> Weights::destroyed(0);

  std::shared_ptr<void> load(int v)
  {
    return std::static_pointer_cast<void>(std::make_shared<Weights>(v));
  }

  int value_of(const std::shared_ptr<void>& held)
  {
    return std::static_pointer_cast<Weights>(held)->value;
  }
}

ZTEST(ModelPool, a_model_is_loaded_once_and_handed_back_after_that)
{
  ModelPool::clear();
  Weights::built = 0;

  std::shared_ptr<void> first  = ModelPool::held("net", [] { return load(7); });
  std::shared_ptr<void> second = ModelPool::held("net", [] { return load(9); });

  ZCHECK_EQ(Weights::built.load(), 1);
  ZCHECK_EQ(value_of(first), 7);

  // The second loader is never run, so the second caller gets the FIRST model.
  // That is the whole point: a request does not get its own copy.
  ZCHECK_EQ(value_of(second), 7);
  ZCHECK(first.get() == second.get());
  ZCHECK_EQ(ModelPool::count(), (size_t)1);

  ModelPool::clear();
}

// The reason held() takes the lock across the load rather than just the lookup.
// Without it, requests arriving together for a model nobody has yet all load it:
// the cost is paid N times and N-1 copies are orphaned.
ZTEST(ModelPool, concurrent_first_callers_load_it_once_between_them)
{
  ModelPool::clear();
  Weights::built = 0;

  const int THREADS = 16;
  std::vector<std::shared_ptr<void> > got(THREADS);
  std::atomic<int> ready(0);

  std::vector<std::thread> threads;
  for (int i = 0; i < THREADS; i++) {
    threads.push_back(std::thread([&, i] {
	  // Line them up, so they arrive at held() together rather than in turn.
	  ready++;
	  while (ready.load() < THREADS) { }
	  got[i] = ModelPool::held("shared", [] { return load(42); });
	}));
  }
  for (size_t i = 0; i < threads.size(); i++) threads[i].join();

  ZCHECK_EQ(Weights::built.load(), 1);
  ZCHECK_EQ(ModelPool::count(), (size_t)1);

  // And every thread got the same one.
  bool all_same = true;
  for (int i = 0; i < THREADS; i++) {
    if (got[i].get() != got[0].get() || value_of(got[i]) != 42) all_same = false;
  }
  ZCHECK(all_same);

  ModelPool::clear();
}

// A model that will not load must not be remembered as a null, or every later
// request gets the null instead of the error that explains it.
ZTEST(ModelPool, a_loader_that_throws_leaves_nothing_behind)
{
  ModelPool::clear();

  ZCHECK_THROWS(ModelPool::held("bad", []() -> std::shared_ptr<void> {
	throw ZiguratException(1, "no such checkpoint");
      }));
  ZCHECK_EQ(ModelPool::count(), (size_t)0);

  // A loader that answers nothing is the same mistake wearing a different hat.
  ZCHECK_THROWS(ModelPool::held("empty", []() -> std::shared_ptr<void> {
	return std::shared_ptr<void>();
      }));
  ZCHECK_EQ(ModelPool::count(), (size_t)0);

  // And the name is still free, so a later attempt gets a real try.
  std::shared_ptr<void> ok = ModelPool::held("bad", [] { return load(3); });
  ZCHECK_EQ(value_of(ok), 3);

  ModelPool::clear();
}

ZTEST(ModelPool, the_ceiling_refuses_rather_than_evicting)
{
  ModelPool::clear();
  ModelPool::limit(2);

  ModelPool::held("a", [] { return load(1); });
  ModelPool::held("b", [] { return load(2); });
  ZCHECK_EQ(ModelPool::count(), (size_t)2);

  // Refused, and nothing already held is thrown away to make room -- what to
  // discard is a policy this does not have.
  ZCHECK_THROWS(ModelPool::held("c", [] { return load(3); }));
  ZCHECK_EQ(ModelPool::count(), (size_t)2);
  ZCHECK(ModelPool::peek("a") != nullptr);
  ZCHECK(ModelPool::peek("b") != nullptr);

  // An existing name is a replacement, not a new entry, so it is allowed.
  ZCHECK_NOTHROW(ModelPool::keep("a", load(11)));
  ZCHECK_EQ(value_of(ModelPool::peek("a")), 11);
  ZCHECK_EQ(ModelPool::count(), (size_t)2);

  // Let one go and the next fits.
  ZCHECK(ModelPool::release("b"));
  ZCHECK_NOTHROW(ModelPool::held("c", [] { return load(3); }));
  ZCHECK_EQ(ModelPool::count(), (size_t)2);

  ModelPool::limit(32);
  ModelPool::clear();
}

// Releasing drops the pool's reference. Anything still holding one keeps
// working -- which is what makes it safe to release a model a request is using.
ZTEST(ModelPool, a_released_model_stays_alive_for_whoever_still_holds_it)
{
  ModelPool::clear();
  Weights::destroyed = 0;

  std::shared_ptr<void> mine = ModelPool::held("net", [] { return load(5); });
  ZCHECK(ModelPool::release("net"));
  ZCHECK_EQ(ModelPool::count(), (size_t)0);

  // Still usable, and still not destroyed.
  ZCHECK_EQ(Weights::destroyed.load(), 0);
  ZCHECK_EQ(value_of(mine), 5);

  mine.reset();
  ZCHECK_EQ(Weights::destroyed.load(), 1);

  // Releasing something that was never there is false rather than an error.
  ZCHECK(!ModelPool::release("never"));

  ModelPool::clear();
}

ZTEST(ModelPool, peek_and_names_report_without_loading)
{
  ModelPool::clear();
  Weights::built = 0;

  ZCHECK(ModelPool::peek("absent") == nullptr);
  ZCHECK_EQ(Weights::built.load(), 0);

  ModelPool::keep("one", load(1));
  ModelPool::keep("two", load(2));

  std::vector<std::string> names = ModelPool::names();
  ZCHECK_EQ(names.size(), (size_t)2);
  ZCHECK_STR(names[0], "one");     // a map, so they come back sorted
  ZCHECK_STR(names[1], "two");

  ModelPool::clear();
  ZCHECK_EQ(ModelPool::count(), (size_t)0);
}
