#include "modelpool.hpp"
#include "zexception.hpp"

#include <map>
#include <mutex>

namespace Zigurat
{

  namespace
  {
    // Function-local rather than file-static, so the map is built the first time
    // it is asked for rather than at some point during static initialisation
    // whose order relative to a caller in another translation unit is not
    // defined. An object's static initialiser can reach this -- that is how an
    // index registers itself -- so the ordering is not hypothetical.
    std::map<std::string, std::shared_ptr<void> >& models()
    {
      static std::map<std::string, std::shared_ptr<void> > held;
      return held;
    }

    std::mutex& access()
    {
      static std::mutex m;
      return m;
    }

    size_t& ceiling()
    {
      static size_t limit = 32;
      return limit;
    }
  }

  std::shared_ptr<void> ModelPool::held(const std::string& name,
					const std::function<std::shared_ptr<void> ()>& load)
  {
    // Held across the load as well as the lookup. Two requests arriving together
    // for a model nobody has yet must not both load it.
    std::lock_guard<std::mutex> lock(access());

    auto iter = models().find(name);
    if (iter != models().end()) return iter->second;

    if (!load) {
      throw ZiguratException(7810, "no loader given for model '" + name + "'");
    }

    if (models().size() >= ceiling()) {
      throw ZiguratException(7811, "the model pool is full at " + std::to_string(ceiling()) +
			     "; '" + name + "' was not loaded");
    }

    // Outside the map until it is built. A loader that throws leaves nothing
    // behind, so the next request tries again and gets the same error rather
    // than a remembered null.
    std::shared_ptr<void> model = load();
    if (!model) {
      throw ZiguratException(7812, "the loader for model '" + name + "' answered nothing");
    }

    models().insert({name, model});
    return model;
  }

  std::shared_ptr<void> ModelPool::peek(const std::string& name)
  {
    std::lock_guard<std::mutex> lock(access());
    auto iter = models().find(name);
    return (iter != models().end()) ? iter->second : std::shared_ptr<void>();
  }

  void ModelPool::keep(const std::string& name, const std::shared_ptr<void>& model)
  {
    std::lock_guard<std::mutex> lock(access());

    // A replacement is not a new entry, so it is allowed at the ceiling.
    if (models().find(name) == models().end() && models().size() >= ceiling()) {
      throw ZiguratException(7811, "the model pool is full at " + std::to_string(ceiling()) +
			     "; '" + name + "' was not kept");
    }

    models()[name] = model;
  }

  bool ModelPool::release(const std::string& name)
  {
    std::lock_guard<std::mutex> lock(access());
    return models().erase(name) > 0;
  }

  void ModelPool::clear()
  {
    std::lock_guard<std::mutex> lock(access());
    models().clear();
  }

  size_t ModelPool::count()
  {
    std::lock_guard<std::mutex> lock(access());
    return models().size();
  }

  size_t ModelPool::limit()
  {
    std::lock_guard<std::mutex> lock(access());
    return ceiling();
  }

  void ModelPool::limit(size_t value)
  {
    std::lock_guard<std::mutex> lock(access());

    // Lowering it below what is already held is allowed and keeps them: the
    // alternative is choosing which to throw away, which is the policy this
    // deliberately does not have. It stops anything new being loaded until
    // enough have been released.
    ceiling() = value;
  }

  std::vector<std::string> ModelPool::names()
  {
    std::lock_guard<std::mutex> lock(access());

    std::vector<std::string> all;
    all.reserve(models().size());
    for (const auto& entry : models()) all.push_back(entry.first);
    return all;
  }

}
