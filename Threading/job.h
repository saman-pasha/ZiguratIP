
#ifndef __JOB_H__
#define __JOB_H__

#include <functional>

namespace Zigurat
{

  class Job : public std::function<void ()>
  {
  public:
    using std::function<void ()>::function;
  };

}

#endif // __JOB_H__
