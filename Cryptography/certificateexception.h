
#ifndef __CERTIFICATEEXCEPTION_H__
#define __CERTIFICATEEXCEPTION_H__

#include "zexception.h"
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

#endif // __CERTIFICATIONEXCEPTION_H__

