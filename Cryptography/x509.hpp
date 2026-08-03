
#ifndef __X509_HPP__
#define __X509_HPP__

#include <string>
#include <iostream>
#include "binarystream.hpp"

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

    // The pieces a certificate carries, for anything that has to act on one --
    // TLS most of all, which encrypts to a peer's key, checks the signature a
    // peer made with it, and will one day decide what a peer may do from who the
    // certificate says it is.
    //
    // certificate_public_key yields a DER SubjectPublicKeyInfo, the same shape
    // the .pub files hold, so it drops straight into validate_by_puk and verify.
    static void        certificate_public_key(binarystream&, binarystream&);
    static std::string certificate_subject(binarystream&);

    // Sign and check an arbitrary message. sign takes a private key file and its
    // pass phrase; verify takes a certificate and reads the key out of it.
    static void sign  (binarystream&, std::string, std::string, binarystream&, binarystream&);
    static bool verify(binarystream&, std::string, binarystream&, binarystream&);

    // RSA key transport: encrypt to the key a certificate names, decrypt with
    // the private half. What a handshake moves a pre master secret with. The
    // plain text has to fit the modulus with room for PKCS #1 padding.
    static void encrypt(binarystream&, binarystream&, binarystream&);
    static void decrypt(binarystream&, std::string, binarystream&, binarystream&);

  };

}

#endif // __X509_HPP__
