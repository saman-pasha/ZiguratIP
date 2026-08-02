
#ifndef __CERTIFICATEEXCEPTION_HPP__
#define __CERTIFICATEEXCEPTION_HPP__

#include "zexception.hpp"
#include <sstream>

namespace Zigurat 
{

  class CertificateException : public ZiguratException
  {
  public:
    CertificateException() = delete;
    CertificateException(std::string msg) : ZiguratException(6579, msg) { }
  };

}

#endif // __CERTIFICATIONEXCEPTION_HPP__

