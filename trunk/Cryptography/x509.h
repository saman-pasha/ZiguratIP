
#ifndef __X509_H__
#define __X509_H__

#include <string>
#include <iostream>
#include "binarystream.h"

namespace Zigurat
{

  class X509
  {
  protected:
    static size_t _generate (std::string, binarystream&, binarystream&, binarystream&);
    static void   _encrypt  (std::string, std::string, binarystream&, binarystream&, binarystream&);
    static void   _decrypt  (binarystream&, std::string, binarystream&, binarystream&);
    static void   _signature_algorithm_id(binarystream&, std::string, binarystream&);
    static void   _sign     (binarystream&, binarystream&, binarystream&, binarystream&);
    static void   _verify   (binarystream&, binarystream&, binarystream&, binarystream&);
    static void   _load_pik_info(binarystream&, std::string, binarystream&, binarystream* = nullptr);
    static void   _load_puk_info(binarystream&, binarystream&);
    static void   _load_csr (binarystream&, binarystream&);
    static void   _load_certificate(binarystream&, binarystream&);
    static void   _dump_pik_info(binarystream&, std::string, std::string, std::string, binarystream&);
    static void   _dump_puk_info(binarystream&, std::string, binarystream&);
    static void   _dump_csr (binarystream&, std::string, binarystream&);
    static void   _dump_certificate(binarystream&, std::string, binarystream&);
    static void   _extract_pik_info(binarystream&, binarystream&, binarystream&);
    static void   _attribute(binarystream&, std::string, std::string);
    static void   _name     (binarystream&, binarystream&);

  public:
    static size_t keygen(std::string, std::string, std::string, std::string, binarystream&, binarystream&);
    static void   csr   (binarystream&, binarystream&, std::string, std::string, std::string, binarystream&);
    static void   issue (binarystream&, binarystream&, binarystream&, std::string, time_t, time_t, binarystream&, std::string, std::string, binarystream&);
    static void   validate_by_pik(binarystream&, std::string, binarystream&);
    static void   validate_by_puk(binarystream&, binarystream&);

  };

}

#endif // __X509_H__
