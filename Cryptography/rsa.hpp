
#ifndef __RSA_HPP__
#define __RSA_HPP__


#include <cstdint>
#include <iostream>
#include <vector>
#include "bigint.hpp"
#include "shahelper.hpp"

namespace Zigurat
{

  class RSA                          // Rivest–Shamir–Adleman Cryptosystem
  {
  public:
    typedef BigInt mod_t;            // RSA modulus type

  protected:
    static const std::vector<mod_t> primes; // First 2000 Prime Integers

  public:
    const int64_t N;                        // Bit-Length of Modulus
    const int64_t k;                        // Length of Modulus in octet
    const SHA::Version HV;                  // SHA Hash Function
    const int64_t hLen;                     // Hash Function Output Length
    const int64_t sLen;                     // PSS Salt Length

    RSA() = delete;
    RSA(int64_t, SHA::Version);
    
    mod_t   PG(const mod_t&, mod_t, mod_t, size_t&);                                     // Prime Generation
    size_t  RSAKG(const mod_t&, mod_t&, mod_t&, mod_t&, mod_t&);                         // Key Generation (e, p, q, n, d)
    size_t  RSAKG(const mod_t&, mod_t&, mod_t&, mod_t&, mod_t&, mod_t&, mod_t&, mod_t&); // Key Generation (e, p, q, dP, dQ, qInv, n, d)
 
    void    I2OSP(const mod_t&, int64_t, uint8_t*);  // Integer To Octet String primitive
    mod_t   OS2IP(const uint8_t*, int64_t);          // Octet String To Integer primitive
    
    mod_t   RSAEP(const mod_t&, const mod_t&, const mod_t&);                                           // RSA Encryption Primitive
    mod_t   RSADP(const mod_t&, const mod_t&, const mod_t&);                                           // RSA Decryption Primitive
    mod_t   RSADP(const mod_t&, const mod_t&, const mod_t&, const mod_t&, const mod_t&, const mod_t&); // RSA Decryption Primitive

    mod_t   RSASP1(const mod_t&, const mod_t&, const mod_t&);                                           // RSA Signature Primitive
    mod_t   RSASP1(const mod_t&, const mod_t&, const mod_t&, const mod_t&, const mod_t&, const mod_t&); // RSA Signature Primitive
    mod_t   RSAVP1(const mod_t&, const mod_t&, const mod_t&);                                           // RSA Verification Primitive

    void    EME_OAEP_Encode(const uint8_t*, int64_t, const uint8_t*, int64_t, uint8_t*);                  // OAEP Encoding Operation
    int64_t EME_OAEP_Decode(const uint8_t*, const uint8_t*, int64_t, uint8_t*);                           // OAEP Decoding Operation
    void    RSAES_OAEP_Encrypt(mod_t, mod_t, const uint8_t*, int64_t, const uint8_t*, int64_t, uint8_t*); // OAEP Encryption Operation
    int64_t RSAES_OAEP_Decrypt(mod_t, mod_t, const uint8_t*, const uint8_t*, int64_t, uint8_t*);          // OAEP Decryption Operation

    void    RSAES_PKCS1_V1_5_Encrypt(mod_t, mod_t, const uint8_t*, int64_t, uint8_t*); // PKCS #1 v1.5 Encryption Operation
    int64_t RSAES_PKCS1_V1_5_Decrypt(mod_t, mod_t, const uint8_t*, uint8_t*);          // PKCS #1 v1.5 Decryption Operation

    void    EMSA_PSS_Encode(const uint8_t*, int64_t, int64_t, int64_t, uint8_t*);                  // PSS Signature Encoding Operation
    void    EMSA_PSS_Verify(const uint8_t*, int64_t, int64_t, int64_t, const uint8_t*);            // PSS Signature Verification Operation
    void    RSASSA_PSS_Sign(mod_t, mod_t, const uint8_t*, int64_t, uint8_t*);                      // PSS Signature Generation Operation
    void    RSASSA_PSS_Sign(mod_t, mod_t, mod_t, mod_t, mod_t, const uint8_t*, int64_t, uint8_t*); // PSS Signature Generation Operation
    void    RSASSA_PSS_Verify(mod_t, mod_t, const uint8_t*, int64_t, uint8_t*);                    // PSS Signature Verification Operation

    void    EMSA_PKCS1_V1_5_Encode(const uint8_t*, int64_t, int64_t, uint8_t*);                           // PKCS #1 v1.5 Sig. Encoding
    void    RSASSA_PKCS1_V1_5_Sign(const mod_t&, const mod_t&, const uint8_t*, int64_t, uint8_t*);        // PKCS #1 v1.5 Sig. Generation
    void    RSASSA_PKCS1_V1_5_Sign(const mod_t&, const mod_t&, std::istream&, size_t, std::ostream&);     // PKCS #1 v1.5 Sig. Generation
    void    RSASSA_PKCS1_V1_5_Sign(const mod_t&, const mod_t&, const mod_t&, const mod_t&, const mod_t&, const uint8_t*, int64_t, uint8_t*);
    void    RSASSA_PKCS1_V1_5_Verify(const mod_t&, const mod_t&, const uint8_t*, int64_t, const uint8_t*);// PKCS #1 v1.5 Sig. Verification
    void    RSASSA_PKCS1_V1_5_Verify(const mod_t&, const mod_t&, std::istream&, size_t, std::istream&);   // PKCS #1 v1.5 Sig. Verification

    void    Hash(const uint8_t*, int64_t, uint8_t*);                // Hash Function  
    void    MGF(const uint8_t*, int64_t, int64_t, uint8_t*);        // Mask Generation Function
    void    XOR(const uint8_t*, const uint8_t*, int64_t, uint8_t*); // XOR Encoding Operation
    void    SGF(int64_t, uint8_t*);                                 // Seed Generation Function
  };

}

#endif // __RSA_HPP__
