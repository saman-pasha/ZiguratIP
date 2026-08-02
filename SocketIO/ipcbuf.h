
#ifndef __IPCBUF_H__
#define __IPCBUF_H__

#include "socketbuf.h"

namespace Zigurat
{

  class ipcbuf : public socketbuf
  {
  protected:
    static const int BUFFER_SIZE;

  public:
    ipcbuf();
    ipcbuf(const ipcbuf&) = delete;
    ipcbuf(ipcbuf&&);

    ipcbuf& operator=(const ipcbuf&) = delete;
    ipcbuf& operator=(ipcbuf&&);
    
    virtual ipcbuf* open(Socket::handle_t, bool = true, int = 0);
    virtual ipcbuf* open(std::string, std::string, bool = true, int = 0);
    virtual bool is_open() const;
    virtual ipcbuf* close();

    virtual ~ipcbuf();
  };

}

#endif // __IPCBUF_H__
