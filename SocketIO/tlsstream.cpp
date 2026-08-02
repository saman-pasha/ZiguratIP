#include "tlsstream.hpp"


namespace Zigurat
{

  tlsstream::tlsstream()
    : nbostream(new tlsbuf())
  {

  }
  
  tlsstream::tlsstream(TLS::ConnectionEnd entity, TLS::HandshakeParameters params, Socket::handle_t handle, bool blocking_mode, int timeout)
    : nbostream(new tlsbuf())
  {
    dynamic_cast<tlsbuf*>(this->rdbuf())->open(entity, params, handle, blocking_mode, timeout);
  }
  
  tlsstream::tlsstream(TLS::HandshakeParameters params, std::string node, std::string service, bool blocking_mode, int timeout)
    : nbostream(new tlsbuf())
  {
    dynamic_cast<tlsbuf*>(this->rdbuf())->open(params, node, service, blocking_mode, timeout);
  }

  tlsstream& tlsstream::operator=(tlsstream&& other)
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();

    this->rdbuf(other.rdbuf());
    other.rdbuf(nullptr);

    return *this;
  }

  void tlsstream::open(TLS::ConnectionEnd entity, TLS::HandshakeParameters params, Socket::handle_t handle, bool blocking_mode, int timeout)
  {
    dynamic_cast<tlsbuf*>(this->rdbuf())->close();
    dynamic_cast<tlsbuf*>(this->rdbuf())->open(entity, params, handle, blocking_mode, timeout);
  }
  
  void tlsstream::open(TLS::HandshakeParameters params, std::string node, std::string service, bool blocking_mode, int timeout)
  {
    dynamic_cast<tlsbuf*>(this->rdbuf())->close();
    dynamic_cast<tlsbuf*>(this->rdbuf())->open(params, node, service, blocking_mode, timeout);
  }

  bool tlsstream::is_open() const
  {
    return dynamic_cast<tlsbuf*>(this->rdbuf())->is_open();
  }
  
  void tlsstream::close()
  {
    dynamic_cast<tlsbuf*>(this->rdbuf())->close();
  }

  void tlsstream::swap(tlsstream& other)
  {
    std::basic_streambuf<char>* tmp = other.rdbuf();
    other.rdbuf(this->rdbuf());
    this->rdbuf(tmp);
  }

  tlsstream::~tlsstream()
  {
    if (this->rdbuf() != nullptr) delete this->rdbuf();
  }

}
