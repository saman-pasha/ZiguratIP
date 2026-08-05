#include "ztest.hpp"
#include "shahelper.hpp"
#include "rsa.hpp"
#include "x509.hpp"
#include "der.hpp"
#include "filestream.hpp"
#include <fstream>
#include <string>
#include "utility.hpp"
#include "bufferstream.hpp"
#include <cstring>

using namespace Zigurat;


// Known answers from FIPS 180-4 / RFC 3174, so a refactor of the digest cores
// cannot quietly change the output.
ZTEST(Cryptography, sha1_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA1, "")),
	     "da39a3ee5e6b4b0d3255bfef95601890afd80709");
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA1, "abc")),
	     "a9993e364706816aba3e25717850c26c9cd0d89d");
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA1, "The quick brown fox jumps over the lazy dog")),
	     "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}

ZTEST(Cryptography, sha224_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA224, "")),
	     "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f");
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA224, "abc")),
	     "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7");
}

ZTEST(Cryptography, sha256_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA256, "")),
	     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA256, "abc")),
	     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

ZTEST(Cryptography, sha384_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA384, "abc")),
	     "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
	     "8086072ba1e7cc2358baeca134c825a7");
}

ZTEST(Cryptography, sha512_known_answers)
{
  ZCHECK_STR(Utility::to_lower(SHA::checksum(SHA::SHA512, "abc")),
	     "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
	     "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
}

ZTEST(Cryptography, digest_sizes)
{
  ZCHECK_EQ(SHA::size(SHA::SHA1), (size_t)20);
  ZCHECK_EQ(SHA::size(SHA::SHA224), (size_t)28);
  ZCHECK_EQ(SHA::size(SHA::SHA256), (size_t)32);
  ZCHECK_EQ(SHA::size(SHA::SHA384), (size_t)48);
  ZCHECK_EQ(SHA::size(SHA::SHA512), (size_t)64);
  ZCHECK(SHA::name(SHA::SHA256).size() > 0);
}

ZTEST(Cryptography, digest_is_stable_and_distinguishing)
{
  const std::string a = SHA::checksum(SHA::SHA256, "ZiguratIP");
  const std::string b = SHA::checksum(SHA::SHA256, "ZiguratIP");
  const std::string c = SHA::checksum(SHA::SHA256, "ZiguratIQ");

  ZCHECK_STR(a, b);
  ZCHECK(a != c);
}

ZTEST(Cryptography, raw_octet_digest_matches_the_string_form)
{
  const std::string data = "abc";
  uint8_t digest[32];
  std::memset(digest, 0, sizeof(digest));

  SHA::checksum(SHA::SHA256, (const uint8_t*)data.c_str(), data.size(), digest);

  ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex(digest, 32)),
	     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

ZTEST(Cryptography, stream_digest_matches_the_buffer_digest)
{
  bufferstream input("abc");
  bufferstream output;

  SHA::checksum(SHA::SHA256, input, 3, output);

  const std::string raw = output.string();
  ZCHECK_EQ(raw.size(), (size_t)32);
  ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex((const uint8_t*)raw.data(), raw.size())),
	     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// RFC 4231 test case 1 (and the RFC 2202 SHA-1 equivalent). Note the argument
// order is (text, key), following the IETF reference implementation.
// RFC 4231, and RFC 2202 for SHA-1. The argument order is the point as much as
// the arithmetic: HMAC(key, message) is not HMAC(message, key), and every TLS
// caller was passing them the other way round because the signature invited it.
ZTEST(Cryptography, hmac_known_answers)
{
  // Case 1: a 20 octet key of 0x0b over "Hi There".
  {
    uint8_t key[20];
    std::memset(key, 0x0b, sizeof(key));
    const std::string data = "Hi There";

    uint8_t mac256[32];
    SHA::hmac(SHA::SHA256, key, sizeof(key), (const uint8_t*)data.c_str(), data.size(), mac256);
    ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex(mac256, 32)),
	       "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");

    uint8_t mac1[20];
    SHA::hmac(SHA::SHA1, key, sizeof(key), (const uint8_t*)data.c_str(), data.size(), mac1);
    ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex(mac1, 20)),
	       "b617318655057264e28bc0b6fb378c8ef146be00");
  }

  // Case 2: a key shorter than the block, and a longer message.
  {
    const std::string key = "Jefe";
    const std::string data = "what do ya want for nothing?";
    uint8_t mac[32];
    SHA::hmac(SHA::SHA256, (const uint8_t*)key.c_str(), key.size(),
	      (const uint8_t*)data.c_str(), data.size(), mac);
    ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex(mac, 32)),
	       "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
  }

  // Case 3: 50 octets of 0xdd under a 20 octet key of 0xaa.
  {
    uint8_t key[20], data[50];
    std::memset(key, 0xaa, sizeof(key));
    std::memset(data, 0xdd, sizeof(data));
    uint8_t mac[32];
    SHA::hmac(SHA::SHA256, key, sizeof(key), data, sizeof(data), mac);
    ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex(mac, 32)),
	       "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
  }

  // Case 6: a key longer than the 64 octet block, which has to be hashed first.
  {
    uint8_t key[131];
    std::memset(key, 0xaa, sizeof(key));
    const std::string data = "Test Using Larger Than Block-Size Key - Hash Key First";
    uint8_t mac[32];
    SHA::hmac(SHA::SHA256, key, sizeof(key), (const uint8_t*)data.c_str(), data.size(), mac);
    ZCHECK_STR(Utility::to_lower(Utility::octet_as_hex(mac, 32)),
	       "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
  }
}

ZTEST(Cryptography, unknown_digest_version_is_rejected)
{
  ZCHECK_THROWS(SHA::size((SHA::Version)999));
}


// ---------------------------------------------------------------------------
// PKCS #1 v1.5 signature encoding
// ---------------------------------------------------------------------------

// RFC 8017 9.2 spells the DigestInfo prefixes out byte for byte, and they carry
// the OID of the HASH. These used to hold 1.2.840.113549.1.1.x -- the
// sha256WithRSAEncryption family, which belongs in a certificate's
// signatureAlgorithm field. Both OIDs are nine octets, so the block was the
// right shape and verified against itself; every other implementation read the
// DigestInfo, failed to recognise it, and rejected the signature.
ZTEST(Cryptography, pkcs1_v1_5_digest_info_carries_the_hash_oid)
{
  struct Case
  {
    SHA::Version version;
    size_t       prefix_length;
    const char*  prefix;      // hex, from RFC 8017 9.2
  };

  static const Case cases[] = {
    {SHA::SHA1,   15, "3021300906052B0E03021A05000414"},
    {SHA::SHA224, 19, "302D300D06096086480165030402040500041C"},
    {SHA::SHA256, 19, "3031300D060960864801650304020105000420"},
    {SHA::SHA384, 19, "3041300D060960864801650304020205000430"},
    {SHA::SHA512, 19, "3051300D060960864801650304020305000440"}
  };

  const uint8_t message[] = {'a', 'b', 'c'};

  for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
    RSA rsa(2048, cases[c].version);

    uint8_t block[256];
    std::memset(block, 0, sizeof(block));
    ZCHECK_NOTHROW(rsa.EMSA_PKCS1_V1_5_Encode(message, sizeof(message), sizeof(block), block));

    // 00 01 then padding, so the DigestInfo begins after the 00 separator.
    ZCHECK_EQ((int)block[0], 0);
    ZCHECK_EQ((int)block[1], 1);

    size_t at = 2;
    while (at < sizeof(block) && block[at] == 0xFF) at++;
    ZCHECK(at > 10);                       // at least 8 octets of padding
    ZCHECK_EQ((int)block[at], 0);
    at++;

    static const char* digits = "0123456789ABCDEF";
    std::string prefix;
    for (size_t i = 0; i < cases[c].prefix_length && at + i < sizeof(block); i++) {
      prefix += digits[block[at + i] >> 4];
      prefix += digits[block[at + i] & 0x0F];
    }
    ZCHECK_STR(prefix, cases[c].prefix);
  }
}


// ---------------------------------------------------------------------------
// What a certificate carries
// ---------------------------------------------------------------------------

namespace
{
  // The shipped sample material, wherever the tests are run from.
  std::string certificate_file(const std::string& name)
  {
    std::string found = Utility::config_path("cert/" + name);
    if (found.size() > 0) return found;

    const char* candidates[] = {"home/etc/cert/", "../home/etc/cert/"};
    for (int i = 0; i < 2; i++) {
      std::ifstream probe(std::string(candidates[i]) + name);
      if (probe.good()) return std::string(candidates[i]) + name;
    }
    return "";
  }
}

// TLS has to encrypt to a peer's key and check what the peer signed with it, so
// the key has to come back out of the certificate. Verified byte for byte
// against "openssl x509 -pubkey" when this was written.
ZTEST(Cryptography, a_certificates_public_key_can_be_read_back_out)
{
  const std::string path = certificate_file("dont-use-certificate.crt");
  if (path.empty()) { ZCHECK(false); return; }

  filestream crt(path, std::ios::in | std::ios::binary);
  ZCHECK(crt.good());

  bufferstream puk_info;
  ZCHECK_NOTHROW(X509::certificate_public_key(crt, puk_info));
  ZCHECK(puk_info.length() > 0);

  // It is a SubjectPublicKeyInfo, so validate_by_puk takes it as it stands:
  // the sample certificate is self signed, so its own key checks its signature.
  bufferstream puk_der;
  DER::encode_sequence(puk_der, puk_info);

  filestream crt_again(path, std::ios::in | std::ios::binary);
  ZCHECK_NOTHROW(X509::validate_by_puk(puk_der, crt_again));
}

// The hook a permission would be keyed on: who the certificate says its holder
// is, rendered the way OpenSSL prints a name.
ZTEST(Cryptography, a_certificates_subject_reads_as_a_distinguished_name)
{
  const std::string path = certificate_file("dont-use-certificate.crt");
  if (path.empty()) { ZCHECK(false); return; }

  filestream crt(path, std::ios::in | std::ios::binary);
  const std::string subject = X509::certificate_subject(crt);

  ZCHECK(subject.find("C=US") != std::string::npos);
  ZCHECK(subject.find("CN=ZiguratIP") != std::string::npos);
  ZCHECK(subject.find("emailAddress=") != std::string::npos);
  ZCHECK(subject.find(", ") != std::string::npos);       // more than one attribute
}

// The whole string, character for character, and not a set of substrings.
//
// This is the one output in the tree where being close is worse than being
// wrong. certificate_subject feeds subject_file_name, which names the files in
// home/etc/users -- so the rendering *is* the registry key. Change a separator
// or an attribute's short name and every registered user silently stops being
// registered: the server starts, the certificates still verify, and every
// connection is refused for a subject nobody can find.
//
// It is written down here because the obvious way to reimplement this is
// OpenSSL's X509_NAME_print_ex, and it does not agree. Its short name for
// givenName (2.5.4.42) is GN, not givenName, and its separator flags are a
// choice rather than a default. Any rewrite has to reproduce the string below,
// whatever it uses underneath.
ZTEST(Cryptography, a_subject_renders_exactly_this_way_and_no_other)
{
  const std::string path = certificate_file("dont-use-certificate.crt");
  if (path.empty()) { ZCHECK(false); return; }

  filestream crt(path, std::ios::in | std::ios::binary);
  ZCHECK_STR(X509::certificate_subject(crt),
	     "C=US, dnQualifier=The Zigurat Informational Platform Project, "
	     "ST=Chicago, CN=ZiguratIP, DC=ziguratip.com, "
	     "emailAddress=info@ziguratip.com");
}

// And the codec that turns that name into a file name, over the characters
// that would otherwise let a subject name a file outside the directory, or
// collide with another subject.
ZTEST(Cryptography, a_subject_survives_being_written_down_as_a_file_name)
{
  struct { const char* dn; const char* file; } cases[] = {
    { "CN=alice",             "CN=alice" },
    { "CN=alice, O=Acme",     "CN=alice%2C%20O=Acme" },
    { "CN=a/b, O=x.y",        "CN=a%2Fb%2C%20O=x%2Ey" },
    { "CN=x=y, O=a,b",        "CN=x=y%2C%20O=a%2Cb" },
    { "emailAddress=a@b.c",   "emailAddress=a%40b%2Ec" },
    { "DC=example, DC=com",   "DC=example%2C%20DC=com" },
    { "CN=\xc3\xa9lise, C=FR", "CN=%C3%A9lise%2C%20C=FR" },
    { "CN=",                  "CN=" },
  };

  for (const auto& c : cases) {
    const std::string encoded = X509::subject_file_name(c.dn);
    ZCHECK_STR(encoded, c.file);
    ZCHECK_STR(X509::file_name_subject(encoded), c.dn);   // and back again
    ZCHECK(encoded.find('/') == std::string::npos);       // never names another directory
  }
}

// Signing an arbitrary message with a private key file, and checking it with
// the certificate that carries the matching public key. This is what proves a
// peer holds the key its certificate names.
ZTEST(Cryptography, a_message_signed_with_a_key_verifies_against_its_certificate)
{
  const std::string crt_path = certificate_file("dont-use-certificate.crt");
  const std::string key_path = certificate_file("dont-use-private.key");
  if (crt_path.empty() || key_path.empty()) { ZCHECK(false); return; }

  const char* text = "a handshake transcript would go here";
  const std::streamsize length = (std::streamsize)std::strlen(text);

  filestream key(key_path, std::ios::in | std::ios::binary);
  bufferstream message, signature;
  message.write(text, length);
  ZCHECK_NOTHROW(X509::sign(key, "", "SHA-256", message, signature));
  ZCHECK(signature.length() > 0);

  filestream crt(crt_path, std::ios::in | std::ios::binary);
  bufferstream same;
  same.write(text, length);
  ZCHECK(X509::verify(crt, "SHA-256", same, signature));

  // A different message under the same signature has to be refused.
  filestream crt_again(crt_path, std::ios::in | std::ios::binary);
  bufferstream tampered;
  tampered.write("a handshake transcript would go HERE", 35);
  ZCHECK(!X509::verify(crt_again, "SHA-256", tampered, signature));
}

// Key transport: a secret encrypted to the key a certificate names, recovered
// with the private half. This is how a handshake moves its pre master secret.
ZTEST(Cryptography, a_secret_encrypted_to_a_certificate_comes_back_with_the_key)
{
  const std::string crt_path = certificate_file("dont-use-certificate.crt");
  const std::string key_path = certificate_file("dont-use-private.key");
  if (crt_path.empty() || key_path.empty()) { ZCHECK(false); return; }

  // 48 octets, the width of a TLS pre master secret.
  uint8_t secret[48];
  for (size_t i = 0; i < sizeof(secret); i++) secret[i] = (uint8_t)(i * 5 + 3);

  bufferstream plain, cipher;
  plain.write((char*)secret, (std::streamsize)sizeof(secret));

  filestream crt(crt_path, std::ios::in | std::ios::binary);
  ZCHECK_NOTHROW(X509::encrypt(crt, plain, cipher));
  ZCHECK(cipher.length() == 256);            // the width of a 2048 bit modulus

  filestream key(key_path, std::ios::in | std::ios::binary);
  bufferstream recovered;
  ZCHECK_NOTHROW(X509::decrypt(key, "", cipher, recovered));

  ZCHECK_EQ((int64_t)recovered.length(), (int64_t)sizeof(secret));
  uint8_t got[48];
  recovered.read((char*)got, 0, (std::streamsize)sizeof(got));
  ZCHECK(std::memcmp(secret, got, sizeof(secret)) == 0);

  // The same secret encrypted twice does not give the same cipher text --
  // PKCS #1 v1.5 padding is randomised, and a fixed one would leak.
  bufferstream plain_again, cipher_again;
  plain_again.write((char*)secret, (std::streamsize)sizeof(secret));
  filestream crt_again(crt_path, std::ios::in | std::ios::binary);
  X509::encrypt(crt_again, plain_again, cipher_again);

  std::string first((size_t)cipher.length(), 0), second((size_t)cipher_again.length(), 0);
  cipher.read(&first[0], 0, cipher.length());
  cipher_again.read(&second[0], 0, cipher_again.length());
  ZCHECK(first != second);
}

// A certificate whose signature does not check out is not one this authority
// issued. This is what a TLS peer is held to.
ZTEST(Cryptography, a_tampered_certificate_fails_validation)
{
  const std::string path = certificate_file("dont-use-certificate.crt");
  if (path.empty()) { ZCHECK(false); return; }

  filestream crt(path, std::ios::in | std::ios::binary);
  bufferstream puk_info, authority_key;
  X509::certificate_public_key(crt, puk_info);
  DER::encode_sequence(authority_key, puk_info);

  // The genuine article passes.
  filestream original(path, std::ios::in | std::ios::binary);
  ZCHECK_NOTHROW(X509::validate_by_puk(authority_key, original));

  // One octet turned over in the body, and it must not.
  std::ifstream source(path, std::ios::binary);
  std::string bytes((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
  ZCHECK(bytes.size() > 200);
  bytes[bytes.size() / 2] = (char)(bytes[bytes.size() / 2] ^ 0x40);

  bufferstream tampered, key_again;
  tampered.write(bytes.data(), (std::streamsize)bytes.size());
  authority_key.read(key_again, 0, authority_key.length());

  bool refused = false;
  try { X509::validate_by_puk(key_again, tampered); }
  catch (...) { refused = true; }
  ZCHECK(refused);
}

// A structure that is not a certificate at all must be refused, not walked off
// the end of. A peer presents its own certificate, so these bytes are hostile
// input; this used to be a bus error.
ZTEST(Cryptography, a_non_certificate_is_refused_rather_than_crashing)
{
  const std::string path = certificate_file("dont-use-public.key");
  if (path.empty()) { ZCHECK(false); return; }

  filestream not_a_certificate(path, std::ios::in | std::ios::binary);
  bufferstream puk_info;
  ZCHECK_THROWS(X509::certificate_public_key(not_a_certificate, puk_info));

  filestream again(path, std::ios::in | std::ios::binary);
  ZCHECK_THROWS(X509::certificate_subject(again));
}

// Permissions the issuer wrote into the certificate, which is where they live
// when there is to be no server-side store of them. Carrying them makes the
// certificate v3: the version field appears, and the extensions after the key.
ZTEST(Cryptography, a_certificate_carries_the_permissions_it_was_issued_with)
{
  const std::string issuer_path = certificate_file("issuer.conf");
  const std::string key_path    = certificate_file("dont-use-private.key");
  if (issuer_path.empty() || key_path.empty()) { ZCHECK(false); return; }

  // A request to issue against, made here so the test needs nothing on disk.
  bufferstream subject_conf;
  subject_conf.write("COUNTRY: US\nCOMMON_NAME: permission-holder\n", 42);

  filestream subject_key(key_path, std::ios::in | std::ios::binary);
  bufferstream csr;
  ZCHECK_NOTHROW(X509::csr(subject_conf, subject_key, "", "SHA-256", "DER", csr));

  const std::vector<std::string> granted {"DEMO", "DEMO::SALES", "BENCH::ITEM"};

  filestream issuer(issuer_path, std::ios::in | std::ios::binary);
  filestream issuer_key(key_path, std::ios::in | std::ios::binary);
  bufferstream serial, certificate;
  serial.write_std_ubyte(0x51);

  ZCHECK_NOTHROW(X509::issue(serial, issuer, issuer_key, "", std::time(0), std::time(0) + 3600,
			     csr, "SHA-256", "DER", granted, certificate));

  bufferstream reading;
  certificate.read(reading, 0, certificate.length());
  const std::vector<std::string> read = X509::certificate_permissions(reading);

  ZCHECK_EQ((int)read.size(), (int)granted.size());
  for (size_t i = 0; i < granted.size() && i < read.size(); i++)
    ZCHECK_STR(read[i], granted[i]);

  // The subject and the key still come out of it, which is what proves the
  // version field is being stepped over rather than read as the serial.
  bufferstream for_subject;
  certificate.read(for_subject, 0, certificate.length());
  ZCHECK(X509::certificate_subject(for_subject).find("CN=permission-holder") != std::string::npos);

  bufferstream for_key, puk_info;
  certificate.read(for_key, 0, certificate.length());
  ZCHECK_NOTHROW(X509::certificate_public_key(for_key, puk_info));
  ZCHECK(puk_info.length() > 0);
}

// Naming none leaves the certificate exactly as it was before any of this
// existed: v1, with no version field and nothing after the key.
ZTEST(Cryptography, a_certificate_without_permissions_stays_v1)
{
  const std::string crt_path = certificate_file("dont-use-certificate.crt");
  if (crt_path.empty()) { ZCHECK(false); return; }

  filestream crt(crt_path, std::ios::in | std::ios::binary);
  const std::vector<std::string> none = X509::certificate_permissions(crt);
  ZCHECK_EQ((int)none.size(), 0);

  // And it is still readable in every other way.
  filestream again(crt_path, std::ios::in | std::ios::binary);
  ZCHECK(X509::certificate_subject(again).find("CN=") != std::string::npos);
}
