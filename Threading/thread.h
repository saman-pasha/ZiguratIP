
#ifndef __THREAD_H__
#define __THREAD_H__

#include <thread>

namespace Zigurat
{

  class Thread : public std::thread
  {
  public:
    using std::thread::thread;
  };

}

#endif // __THREAD_H__
