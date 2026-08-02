
#ifndef __TEXTSTREAM_HPP__
#define __TEXTSTREAM_HPP__

#include <sstream>

namespace Zigurat
{

  class textstream : public std::stringstream
  {
  public:
    using std::stringstream::stringstream;
  };

}

#endif // __TEXTSTREAM_HPP__
