
#ifndef __THREAD_HPP__
#define __THREAD_HPP__

#include <thread>

namespace Zigurat
{

  class Thread : public std::thread
  {
  public:
    using std::thread::thread;
  };

}

#endif // __THREAD_HPP__
