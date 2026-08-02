
#ifndef __JOB_HPP__
#define __JOB_HPP__

#include <functional>

namespace Zigurat
{

  class Job : public std::function<void ()>
  {
  public:
    using std::function<void ()>::function;
  };

}

#endif // __JOB_HPP__
