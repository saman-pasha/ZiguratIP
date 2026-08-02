
#ifndef __IPCSTREAM_H__
#define __IPCSTREAM_H__

#include "hbostream.h"
#include "ipcbuf.h"

namespace Zigurat
{

  class ipcstream : public hbostream
  {
  public:
    using hbostream::hbostream;

    ipcstream();
    explicit ipcstream(Socket::handle_t, bool = true, int = 0);
    explicit ipcstream(std::string, std::string, bool = true, int = 0);

    ipcstream& operator=(const ipcstream&) = delete;
    ipcstream& operator=(ipcstream&&);

    void open(Socket::handle_t, bool = true, int = 0);
    void open(std::string, std::string, bool = true, int = 0);
    bool is_open() const;
    void close();

    void swap(ipcstream&);

    virtual ~ipcstream();
  };

}

#endif // __IPCSTREAM_H__
