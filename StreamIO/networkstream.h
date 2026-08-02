
#ifndef __NETWORKSTREAM_H__
#define __NETWORKSTREAM_H__

#include "nbostream.h"
#include <iostream>

namespace Zigurat
{

  class networkstream : public nbostream<std::iostream>
  {
  public:
    using nbostream<std::iostream>::nbostream;
  };

}

#endif // __NETWORKSTREAM_H__
