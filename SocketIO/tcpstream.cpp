#include "tcpstream.hpp"


namespace Zigurat
{

  tcpstream::tcpstream()
    : nbostream(new tcpbuf())
  {
  }
  
  tcpstream::tcpstream(Socket::handle_t handle, bool blocking_mode, int timeout)
    : nbostream(new tcpbuf())
  {
    dynamic_cast<tcpbuf*>(this->rdbuf())->open(handle, blocking_mode, timeout);
  }
  
  tcpstream::tcpstream(std::string node, std::string service, bool blocking_mode, int timeout)
    : nbostream(new tcpbuf())
  {
    dynamic_cast<tcpbuf*>(this->rdbuf())->open(node, service, blocking_mode, timeout);
  }

  // std::basic_iostream has no default constructor, so the base has to be built
  // with a null buffer before the moved-from one can be adopted.
  tcpstream::tcpstream(tcpstream&& other)
    : nbostream(nullptr)
  {
    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);
  }

  tcpstream& tcpstream::operator=(tcpstream&& other)
  {
    if (this == &other) return *this;

    if (this->rdbuf() != nullptr) delete this->rdbuf();

    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);

    return *this;
  }

  void tcpstream::open(Socket::handle_t handle, bool blocking_mode, int timeout)
  {
    tcpbuf* buf = dynamic_cast<tcpbuf*>(this->rdbuf());
    if (buf == nullptr) throw SocketIOException("tcp stream has no buffer");
    buf->close();
    buf->open(handle, blocking_mode, timeout);
    this->clear();
  }

  void tcpstream::open(std::string node, std::string service, bool blocking_mode, int timeout)
  {
    tcpbuf* buf = dynamic_cast<tcpbuf*>(this->rdbuf());
    if (buf == nullptr) throw SocketIOException("tcp stream has no buffer");
    buf->close();
    buf->open(node, service, blocking_mode, timeout);
    this->clear();
  }

  bool tcpstream::is_open() const
  {
    tcpbuf* buf = dynamic_cast<tcpbuf*>(this->rdbuf());
    return (buf != nullptr) && buf->is_open();
  }

  void tcpstream::close()
  {
    tcpbuf* buf = dynamic_cast<tcpbuf*>(this->rdbuf());
    if (buf != nullptr) buf->close();
  }

  void tcpstream::swap(tcpstream& other)
  {
    std::basic_streambuf<char>* tmp = other.rdbuf();
    other.rdbuf(this->rdbuf());
    this->rdbuf(tmp);
  }

  tcpstream::~tcpstream()
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();
  }

  Socket::handle_t tcpstream::handle() const
  {
    const tcpbuf* buffer = dynamic_cast<const tcpbuf*>(this->rdbuf());
    return (buffer == nullptr) ? Socket::INVALID_SOCKET : buffer->handle();
  }

}
