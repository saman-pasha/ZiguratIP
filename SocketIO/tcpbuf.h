
#ifndef __TCPBUF_H__
#define __TCPBUF_H__

#include "socketbuf.h"

namespace Zigurat
{

  class tcpbuf : public socketbuf
  {
  protected:
    static const int BUFFER_SIZE;

  public:
    tcpbuf();
    tcpbuf(const tcpbuf&) = delete;
    tcpbuf(tcpbuf&&);

    tcpbuf& operator=(const tcpbuf&) = delete;
    tcpbuf& operator=(tcpbuf&&);
    
    virtual tcpbuf* open(Socket::handle_t, bool = true, int = 0);
    virtual tcpbuf* open(std::string, std::string, bool = true, int = 0);
    virtual bool is_open() const;
    virtual tcpbuf* close();

    virtual ~tcpbuf();
  };

}

#endif // __TCPBUF_H__
