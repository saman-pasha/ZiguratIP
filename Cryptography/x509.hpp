
#ifndef __X509_HPP__
#define __X509_HPP__

#include <string>
#include <vector>
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
    static void   issue (binarystream&, binarystream&, binarystream&, std::string, time_t, time_t, binarystream&, std::string, std::string, const std::vector<std::string>&, binarystream&);
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

    // How a distinguished name is written down as one file name, and read back.
    // A DN carries commas, spaces and equals signs, and nothing stops it
    // carrying a slash or a dot -- so everything outside a plain set is percent
    // encoded. The result stays legible, "CN=alice,%20O=Acme", and can never
    // name anything outside the directory it is written in.
    static std::string subject_file_name(const std::string&);
    static std::string file_name_subject(const std::string&);

    // What the holder of this certificate may do, as the issuer wrote it. The
    // strings mean nothing here -- they are matched by whoever cares. An empty
    // list comes back for a certificate carrying no such extension, which is
    // every v1 one and every one issued before this existed.
    static std::vector<std::string> certificate_permissions(binarystream&);

    // 1.3.6.1.4.1.55447.1.1. This arc is not registered with IANA: put your own
    // Private Enterprise Number here if you have one, and reissue.
    static const char* PERMISSIONS_OID;

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
