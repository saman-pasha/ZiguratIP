#include "httpexception.h"


namespace Zigurat
{

  HTTPException::HTTPException(std::string msg)
    :ZiguratException(8779, msg)
  {

  }

}
