
#ifndef __NETWORKSTREAM_HPP__
#define __NETWORKSTREAM_HPP__

#include "nbostream.hpp"
#include <iostream>

namespace Zigurat
{

  class networkstream : public nbostream<std::iostream>
  {
  public:
    using nbostream<std::iostream>::nbostream;
  };

}

#endif // __NETWORKSTREAM_HPP__
