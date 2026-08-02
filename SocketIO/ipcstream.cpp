#include "ipcstream.h"


namespace Zigurat
{

  ipcstream::ipcstream()
    : hbostream(new ipcbuf())
  {
  }
  
  ipcstream::ipcstream(Socket::handle_t handle, bool blocking_mode, int timeout)
    : hbostream(new ipcbuf())
  {
    dynamic_cast<ipcbuf*>(this->rdbuf())->open(handle, blocking_mode, timeout);
  }
  
  ipcstream::ipcstream(std::string node, std::string service, bool blocking_mode, int timeout)
    : hbostream(new ipcbuf())
  {
    dynamic_cast<ipcbuf*>(this->rdbuf())->open(node, service, blocking_mode, timeout);
  }

  ipcstream& ipcstream::operator=(ipcstream&& other)
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();

    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);

    return *this;
  }

  void ipcstream::open(Socket::handle_t handle, bool blocking_mode, int timeout)
  {
    dynamic_cast<ipcbuf*>(this->rdbuf())->close();
    dynamic_cast<ipcbuf*>(this->rdbuf())->open(handle, blocking_mode, timeout);
  }
  
  void ipcstream::open(std::string node, std::string service, bool blocking_mode, int timeout)
  {
    dynamic_cast<ipcbuf*>(this->rdbuf())->close();
    dynamic_cast<ipcbuf*>(this->rdbuf())->open(node, service, blocking_mode, timeout);
  }

  bool ipcstream::is_open() const
  {
    return dynamic_cast<ipcbuf*>(this->rdbuf())->is_open();
  }
  
  void ipcstream::close()
  {
    dynamic_cast<ipcbuf*>(this->rdbuf())->close();
  }

  void ipcstream::swap(ipcstream& other)
  {
    std::basic_streambuf<char>* tmp = other.rdbuf();
    other.rdbuf(this->rdbuf());
    this->rdbuf(tmp);
  }

  ipcstream::~ipcstream()
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();
  }

}
