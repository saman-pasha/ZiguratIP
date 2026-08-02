
#ifndef __TEXTSTREAM_H__
#define __TEXTSTREAM_H__

#include <sstream>

namespace Zigurat
{

  class textstream : public std::stringstream
  {
  public:
    using std::stringstream::stringstream;
  };

}

#endif // __TEXTSTREAM_H__
