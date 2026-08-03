#ifndef __NETWORKSTREAM_HPP__
#define __NETWORKSTREAM_HPP__

#include "nbostream.hpp"
#include <sstream>

namespace Zigurat

{

  // A bufferstream that writes network byte order: the same in-memory buffer,
  // reached through nbostream instead of hbostream.
  //
  // bufferstream is deliberately host order, because what it mostly holds is
  // rows on their way into a page and those must keep the layout the page store
  // reads back. A protocol message is the other case: it is assembled in memory
  // and then handed to a socket, so it has to be laid out the way the wire
  // expects from the moment it is built. Assembling one in a bufferstream and
  // sending the result verbatim -- which is what the TLS handshake did -- puts
  // host order octets on the wire however correct the socket itself is.
  class networkstream : public nbostream
  {
  public:
    using nbostream::nbostream;

    networkstream();
    explicit networkstream(std::ios_base::openmode);
    explicit networkstream(std::basic_string<char>, std::ios_base::openmode = std::ios_base::in | std::ios_base::out);

    networkstream& operator=(const networkstream&) = delete;
    networkstream& operator=(networkstream&&);

    std::basic_string<char> string();
    void string(std::basic_string<char>);

    void swap(networkstream&);

    virtual ~networkstream();
  };

}

#endif // __NETWORKSTREAM_HPP__
