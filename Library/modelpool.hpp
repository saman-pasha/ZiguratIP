#ifndef __MODELPOOL_HPP__
#define __MODELPOOL_HPP__

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Zigurat
{

  // Things that are expensive to build and must outlive the request that first
  // needed one -- a neural network's weights being the case this was written
  // for.
  //
  // WHY IT IS NEEDED AT ALL. A page is constructed per request. A model held as
  // a page member is therefore loaded, used once, and destroyed, and a
  // checkpoint of any size makes that the whole cost of answering: the demo
  // classifier is 946 KB read off disk and parsed by libtorch for one
  // prediction. A model is state plus a method, which is why it is a CLASS and
  // not a PROCEDURE -- but a class the page owns is still a class the page
  // destroys.
  //
  // WHERE IT LIVES, and why a static in the object would nearly do. A compiled
  // object's .so is opened once and kept by LibraryPool, so a function-local
  // static inside it already survives requests. What that does not give you is
  // a ceiling, a way to see what is held, or a way to let go -- and a cache
  // with no bound is a leak that has not been noticed yet. Those are the
  // reasons this is a class and not four lines in a generated fragment.
  //
  // WHAT IT HOLDS. shared_ptr<void>, because this library is built long before
  // anything that would name a model type, and it must not learn about one. The
  // caller knows what it put in and casts it back on the way out; keep() and
  // held() are a matched pair and mixing types under one name is the caller's
  // bug, not something this can check.
  //
  // THREADS. Every worker serves requests on its own thread and they share this,
  // so everything below takes the lock. `held' takes it across the LOAD as well
  // as the lookup, deliberately: two requests arriving together for a model
  // nobody has loaded yet must not both load it, which would cost twice and
  // leave one copy orphaned. The cost is that a slow load blocks other names
  // too, which for something loaded once at first use is the right trade.
  class ModelPool
  {
  public:
    // What NAME holds, loading it with LOAD if nobody has yet.
    //
    // LOAD runs at most once per name. If it throws, nothing is kept and the
    // exception reaches the caller -- a model that cannot be loaded must not be
    // remembered as a null, or every later request gets the null instead of the
    // error that explains it.
    static std::shared_ptr<void> held(const std::string& name,
				      const std::function<std::shared_ptr<void> ()>& load);

    // What NAME holds, or nullptr. Does not load: for a caller that wants to
    // know rather than to have.
    static std::shared_ptr<void> peek(const std::string& name);

    // Put one in directly, replacing whatever was there. For a caller that has
    // already built the thing -- and it is the caller's business whether the
    // replacement invalidates something else's copy, since a shared_ptr already
    // handed out stays valid either way.
    static void keep(const std::string& name, const std::shared_ptr<void>& model);

    // Forget it. Anything still holding the shared_ptr keeps working; this drops
    // the pool's reference, which is usually the last one.
    static bool release(const std::string& name);

    // Forget all of them. For shutdown, and for a test that wants to start from
    // nothing -- without it a suite's second case inherits the first one's.
    static void clear();

    static size_t count();
    static size_t limit();

    // The ceiling, so a name derived from a request cannot grow this without
    // bound. Reaching it is an error rather than an eviction: what to throw away
    // is a policy this cannot guess, and silently dropping a model that a
    // request is about to need is worse than refusing to load a new one.
    static void limit(size_t);

    // What is held, for a DBA report or a test.
    static std::vector<std::string> names();
  };

}

#endif // __MODELPOOL_HPP__
