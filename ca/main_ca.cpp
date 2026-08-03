#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <ctime>
#include "certificateexception.hpp"
#include "utility.hpp"
#include "argument.hpp"
#include "configuration.hpp"
#include "bigint.hpp"
#include "x509.hpp"
#include "rsa.hpp"
#include "bufferstream.hpp"
#include <sstream>


using namespace Zigurat;

namespace
{
  // X509 works on binarystream, so the file plumbing lives here rather than
  // being repeated at every call site.
  void load_stream(std::istream& input, bufferstream& stream)
  {
    std::ostringstream all;
    all << input.rdbuf();
    stream.string(all.str());
    stream.clear();
  }

  void dump_stream(bufferstream& stream, std::ostream& output)
  {
    const std::string content = stream.string();
    output.write(content.data(), (std::streamsize)content.size());
  }
}

void help  ();
void keygen(int, char*[]);
void csr   (int, char*[]);
void issue (int, char*[]);
void pikval(int, char*[]);
void pukval(int, char*[]);

// Where the certificates live. SECURITY/CERTIFICATE_PATH in ziguratip.conf if it
// is set, otherwise ZIGURATIP_HOME/etc/cert. Everything the tool defaults to --
// the issuer configuration and its key pair -- is looked for in there, so moving
// the directory moves all of it at once.
static std::string shipped_certificate(const std::string& file_name)
{
  std::string configured;

  std::string conf = Zigurat::Utility::config_path("ziguratip.conf");
  if (conf.size() > 0) {
    try {
      Zigurat::Configuration config(conf);
      config.get("/SECURITY/CERTIFICATE_PATH", configured);
    } catch (...) {
      // An unreadable configuration is not this tool's problem: fall through to
      // the built in location.
    }
  }

  if (configured.size() > 0) {
    if (configured.back() != '/') configured.push_back('/');
    return configured + file_name;
  }

  return Zigurat::Utility::config_path("cert/" + file_name);
}

int main(int argc, char* argv[])
{
  /*
  RSA rsa(1024, SHA::SHA1);
  uint8_t pb[64] = {0x70, 0xBD, 0xA6, 0xD5, 0x3E, 0xCD, 0x53, 0x35, 0x03, 0x43, 0x5C, 0xBC, 0xFC, 0x8E, 0x93, 0x86, 
		    0x87, 0x1F, 0x30, 0x1E, 0x13, 0x97, 0x5D, 0x85, 0xC3, 0xB3, 0x0B, 0xDA, 0x0C, 0x8C, 0x17, 0x6D, 
		    0x35, 0xE2, 0xB2, 0x20, 0xAE, 0x65, 0x55, 0x7F, 0x1F, 0x1A, 0xA0, 0x14, 0x32, 0xC5, 0xB7, 0x8B, 
		    0x62, 0xB9, 0xFA, 0x1C, 0x8D, 0xF7, 0xDF, 0x51, 0x6F, 0xF8, 0x21, 0x9E, 0xEC, 0x57, 0xF7, 0x65};
  uint8_t qb[64] = {0x5E, 0x0B, 0x45, 0x99, 0x94, 0xAC, 0x88, 0x45, 0x5E, 0x65, 0x44, 0x41, 0x90, 0x3C, 0xC4, 0xFA, 
		    0x93, 0xD9, 0xE6, 0x23, 0x5A, 0x83, 0xE9, 0xA4, 0x66, 0x9D, 0x07, 0xFE, 0xF9, 0xFA, 0x29, 0x53, 
		    0x7B, 0xCD, 0x97, 0x04, 0xEE, 0x39, 0x81, 0x7B, 0x9C, 0xCB, 0xB0, 0x7D, 0x29, 0xDD, 0x25, 0x45, 
		    0xBA, 0x00, 0x56, 0xCF, 0x9C, 0xDB, 0x36, 0x6A, 0xD0, 0xA6, 0xAF, 0xCC, 0xE8, 0xDB, 0xED, 0xCB};
   
  BigInt p(pb, 64, true);
  BigInt q(qb, 64, false);

  BigInt e = 65537;
  BigInt n = p * q, d, _;
  BigInt::eed(e, BigInt::lcm(p - 1, q - 1), d, _);
 
  //rsa.RSAKG(e, p, q, n, d);

  std::cout << "e: " << e << std::endl;
  std::cout << "p: " << p << std::endl;
  std::cout << "q: " << q << std::endl;
  std::cout << "n: " << n << std::endl;
  std::cout << "d: " << d << std::endl;
  
  int64_t k = 128; // (1024 / 8)
  int64_t mLen = 16;
  uint8_t M[mLen] = {0xd4, 0x36, 0xe9, 0x95, 0x69, 0xfd, 0x32, 0xa7, 0xc8, 0xa0, 0x5b, 0xbc, 0x90, 0xd3, 0x2c, 0x49};
  uint8_t C[k];

  Buffer bM, bS;
  bM.write(M, mLen);
  rsa.RSASSA_PKCS1_V1_5_Sign(n, d, bM, bS);
  
  //  rsa.RSASSA_PKCS1_V1_5_Sign(n, d, M, mLen, C);

  std::cout << "C: " << std::hex;
  for (size_t i = 0; i < 128; i++)
    std::cout << (int)C[i] << ' ';
  std::cout << std::endl;

  bM.seek_read(0, std::ios::beg);
  bS.seek_read(0, std::ios::beg);

  //  rsa.RSASSA_PKCS1_V1_5_Verify(n, e, M, mLen, C);
  rsa.RSASSA_PKCS1_V1_5_Verify(n, e, bM, bS);

  std::cout << "M: " << std::hex;
  for (int64_t i = 0; i < mLen; i++)
    std::cout << (int)M[i] << ' ';
  std::cout << std::endl;

  return 0;
  */
  /*
  BigInt a = 1234567890123, b = 6789012345678;
  std::cout << "a: " << a << " b: " << b << std::endl;
  std::cout << (a * a * a * 128) << std::endl;
  std::cout << (a - b) << std::endl;
  std::cout << ((a - b) * a * a * 32) << std::endl;
  
  return 0;
  */
  /*
  BigInt a = 1234567890123, b = 6789012345678;
  clock_t begin_time = std::clock();
  
  for (size_t i = 0; i < 1000; i++) {
    a = a * b;
  }

  clock_t end_time = std::clock();
  //  std::cout << "BigInt: " << a << " " << 1000.0 * (end_time - begin_time) / CLOCKS_PER_SEC << " ms" << std::endl;
  std::cout << "executed in: " << 1000.0 * (end_time - begin_time) / CLOCKS_PER_SEC << " ms" << std::endl;
 
  begin_time = std::clock();

  for (size_t i = 0; i < 1000; i++) {
    a = a / b;
  }
  
  end_time = std::clock();
  std::cout << "BigInt: " << a << " " << 1000.0 * (end_time - begin_time) / CLOCKS_PER_SEC << " ms" << std::endl;
  return 0;
  */
  
  std::cout << "\tZiguratIP X.509 v3 Certification Authority (CA)" << std::endl;
  std::cout << std::endl;

  if (argc == 1 || (argc > 1 && (argv[1][0] == '?' || std::strcmp(argv[1], "help") == 0 || std::strcmp(argv[1], "--help") == 0))) {
    help();
  } else if (argc > 1 && std::strcmp(argv[1], "keygen") == 0) {
    keygen(argc, argv);
  } else if (argc > 1 && std::strcmp(argv[1], "csr") == 0) {
    csr(argc, argv);
  } else if (argc > 1 && std::strcmp(argv[1], "issue") == 0) {
    issue(argc, argv);
  } else if (argc > 1 && std::strcmp(argv[1], "pikval") == 0) {
    pikval(argc, argv);
  } else if (argc > 1 && std::strcmp(argv[1], "pukval") == 0) {
    pukval(argc, argv);
  } else {
    throw CertificateException("invalid instruction " + std::string(argv[1]));
  }

  return 0;
}

void help()
{
  std::cout << "Options: " << std::endl;
  std::cout << "\t--signature        ::= RSA-[1024|2048|3072|4096]" << std::endl;
  std::cout << "\t--hash             ::= SHA-[1|224|256|384|512]" << std::endl;
  std::cout << "\t--encryption       ::= AES-[128|192|256]" << std::endl;
  std::cout << "\t--cipher           ::= \"cipher key\" --! secret key !--" << std::endl;
  std::cout << "\t--encoding         ::= DER|PEM --! binary or base64 !--" << std::endl;
  std::cout << "\t--private          ::= \"private key file\"" << std::endl;
  std::cout << "\t--public           ::= \"public key file\"" << std::endl;
  std::cout << "\t--serial           ::= from 0 to " << std::to_string(std::numeric_limits<size_t>::max()) << " --! certificate serial number !--" << std::endl;
  std::cout << "\t--issuer           ::= \"issuer name configuration file\"" << std::endl;
  std::cout << "\t--issuer-pik       ::= \"issuer private key file\"" << std::endl;
  std::cout << "\t--issuer-puk       ::= \"issuer public key file\"" << std::endl;
  std::cout << "\t--from             ::= \"YYYYMMDDHHMMSS\" --! GMT validity not before !--" << std::endl;
  std::cout << "\t--to               ::= \"YYYYMMDDHHMMSS\" --! GMT validity not after !--" << std::endl;
  std::cout << "\t--subject          ::= \"subject name configuration file\"" << std::endl;
  std::cout << "\t--subject-pik      ::= \"subject private key file\"" << std::endl;
  std::cout << "\t--csr              ::= \"certificate signing request file\"" << std::endl;
  std::cout << "\t--certificate      ::= \"certificate file\"" << std::endl;
  std::cout << std::endl;
  std::cout << "Defaults: " << std::endl;
  std::cout << "\t--signature        ::= RSA-2048" << std::endl;
  std::cout << "\t--hash             ::= SHA-1" << std::endl;
  std::cout << "\t--encoding         ::= DER" << std::endl;
  std::cout << "\t--private          ::= private.key" << std::endl;
  std::cout << "\t--public           ::= public.key" << std::endl;
  std::cout << "\t--serial           ::= random 20 octet positive integer" << std::endl;
  std::cout << "\t--issuer           ::= ZIGURATIP_HOME/etc/cert/issuer.conf" << std::endl;
  std::cout << "\t--issuer-pik       ::= ZIGURATIP_HOME/etc/cert/dont-use-private.key" << std::endl;
  std::cout << "\t--issuer-puk       ::= ZIGURATIP_HOME/etc/cert/dont-use-public.key" << std::endl;
  std::cout << "\t--from             ::= now" << std::endl;
  std::cout << "\t--to               ::= --from + 1 year" << std::endl;
  std::cout << "\t--subject          ::= subject.conf" << std::endl;
  std::cout << "\t--subject-pik      ::= subject.key" << std::endl;
  std::cout << "\t--csr              ::= request.csr" << std::endl;
  std::cout << "\t--certificate      ::= certificate.crt" << std::endl;
  std::cout << std::endl;
  std::cout << "Instructions: " << std::endl;
  std::cout << "\t--! Generating a new Private and Public key pair !--" << std::endl;
  std::cout << "\tkeygen --signature=? --encryption=? --cipher=\"?\" --encoding=? --private=? --public=?" << std::endl;
  std::cout << std::endl;
  std::cout << "\t--! Generating a Certificate Signing Request !--" << std::endl;
  std::cout << "\tcsr --subject=? --subject-pik=? --cipher=\"?\" --hash=? --encoding=? --csr=?" << std::endl;
  std::cout << std::endl;
  std::cout << "\t--! Issuing a Certificate from Certificate Signing Request !--" << std::endl;
  std::cout << "\tissue --serial=? --issuer=? --issuer-pik=? --cipher=\"?\" --from=? --to=? --csr=? --hash=? --encoding=? --certificate=?" << std::endl;
  std::cout << std::endl;
  std::cout << "\t--! Validating an Issued Certificate by issuer private key!--" << std::endl;
  std::cout << "\tpikval --issuer-pik=? --cipher=\"?\" --certificate=?" << std::endl;
  std::cout << std::endl;
  std::cout << "\t--! Validating an Issued Certificate by issuer public key!--" << std::endl;
  std::cout << "\tpukval --issuer-puk=? --certificate=?" << std::endl;
  std::cout << std::endl;
}

void keygen(int argc, char* argv[])
{
  std::cout << "Key Generating Arguments: " << std::endl;

  Zigurat::Argument args(argc, argv);

  // Signature
  std::string signature = args.get("--signature");
  if (signature.size() == 0) signature = "RSA-2048";
  std::cout << "    Signature Algorithm: " << signature << std::endl;

  // Encryption
  std::string encryption, cipher_key;
  if (args.get("--encryption", encryption)) {
    if (!args.get("--cipher", cipher_key)) throw CertificateException("cipher key should be provided");
    std::cout << "   Encryption Algorithm: " << encryption << std::endl;
  }

  // Encoding
  std::string encoding = args.get("--encoding");
  if (encoding.size() == 0) encoding = "DER";
  std::cout << "     Encoding Algorithm: " << encoding << std::endl;

  // Private Key File
  std::string private_key_path = args.get("--private");
  if (private_key_path.size() == 0) private_key_path = "private.key";
  std::ofstream private_key_file(private_key_path, std::ios::trunc);
  if (!private_key_file.good()) {
    throw CertificateException("invalid private key file " + private_key_path);
  }
  std::cout << "            Private Key: " << private_key_path << std::endl;

  // Public Key File
  std::string public_key_path = args.get("--public");
  if (public_key_path.size() == 0) public_key_path = "public.key";
  std::ofstream public_key_file(public_key_path, std::ios::trunc);
  if (!public_key_file.good()) {
    throw CertificateException("invalid public key file " + public_key_path);
  }
  std::cout << "             Public Key: " << public_key_path << std::endl;
  
  // Key Generating
  std::cout << std::endl;
  std::cout << "Key Generating ..." << std::endl;
  bufferstream pik_stream, puk_stream;
  clock_t begin_time = std::clock();
  size_t tries = X509::keygen(signature, encryption, cipher_key, encoding, pik_stream, puk_stream);
  clock_t end_time = std::clock();
  std::cout << "Key Generated in " << 1000.0 * (end_time - begin_time) / CLOCKS_PER_SEC << " ms by " << tries << " tries " << std::endl;

  dump_stream(pik_stream, private_key_file);
  dump_stream(puk_stream, public_key_file);
  private_key_file.close();
  public_key_file.close();
}

void csr(int argc, char* argv[])
{
  std::cout << "Certificate Signing Request Arguments: " << std::endl;

  Argument args(argc, argv);

  // Subject Name Configuration File
  std::string subject_path(args.get("--subject"));
  if (subject_path.size() == 0) subject_path = "subject.conf";
  std::ifstream subject_file(subject_path);
  if (!subject_file.good()) {
    throw CertificateException("invalid subject configuration file " + subject_path);
  }
  std::cout << "    Subject Name Config: " << subject_path << std::endl;

  // Subject Private Key File
  std::string subject_pik_path(args.get("--subject-pik"));
  if (subject_pik_path.size() == 0) subject_pik_path = "subject.key";
  std::ifstream subject_pik_file(subject_pik_path);
  if (!subject_pik_file.good()) {
    throw CertificateException("invalid subject private key file " + subject_pik_path);
  }
  std::cout << "    Subject Private Key: " << subject_pik_path << std::endl;

  // Encryption Cipher Key
  std::string cipher_key = args.get("--cipher");

  // Hash
  std::string hash(args.get("--hash"));
  if (hash.size() == 0) hash = "SHA-1";
  std::cout << "         Hash Algorithm: " << hash << std::endl;

  // Encoding
  std::string encoding = args.get("--encoding");
  if (encoding.size() == 0) encoding = "DER";
  std::cout << "     Encoding Algorithm: " << encoding << std::endl;

  // Certificate Signin Request File
  std::string csr_path(args.get("--csr"));
  if (csr_path.size() == 0) csr_path = "request.csr";
  std::ofstream csr_file(csr_path, std::ios::trunc);
  if (!csr_file.good()) {
    throw CertificateException("invalid csr file " + csr_path);
  }
  std::cout << "    Certificate Request: " << csr_path << std::endl;

  // Generating CSR
  std::cout << std::endl;
  std::cout << "Generating CSR ..." << std::endl;
  bufferstream subject_stream, pik_stream, csr_stream;
  load_stream(subject_file, subject_stream);
  load_stream(subject_pik_file, pik_stream);
  X509::csr(subject_stream, pik_stream, cipher_key, hash, encoding, csr_stream);
  dump_stream(csr_stream, csr_file);
  csr_file.close();
  std::cout << "CSR Generated" << std::endl;
}

void issue(int argc, char* argv[])
{
  std::cout << "Certificate Issuing Arguments: " << std::endl;

  Argument args(argc, argv);

  // Certificate Serial Number
  std::string serial_number_string;
  bufferstream serial_number;
  if (args.get("--serial", serial_number_string)) {
    BigInt number = (uint64_t)std::stoull(serial_number_string);
    number.to_octet_string(serial_number, true);    
    std::cout << "          Serial Number: " << number << std::endl;
  } else {
    BigInt number = BigInt::rand(20 / sizeof(typename BigInt::word_t));
    number.to_octet_string(serial_number, true);
    std::cout << "          Serial Number: " << number << std::endl;
  }

  // Issuer Configuration File
  std::string issuer_path;
  if (!args.get("--issuer", issuer_path)) {
    issuer_path = shipped_certificate("issuer.conf");
  }
  std::ifstream issuer_file(issuer_path);
  if (!issuer_file.good()) {
    throw CertificateException("invalid issuer configuration file " + issuer_path);
  }
  std::cout << "     Issuer Name Config: " << issuer_path << std::endl;

  // Issuer Private Key File
  std::string issuer_key_path;
  if (!args.get("--issuer-pik", issuer_key_path)) {
    issuer_key_path = shipped_certificate("dont-use-private.key");
  }
  std::ifstream issuer_key_file(issuer_key_path);
  if (!issuer_key_file.good()) {
    throw CertificateException("invalid issuer private key file " + issuer_key_path);
  }
  std::cout << "     Issuer Private Key: " << issuer_key_path << std::endl;

  // Encryption Cipher Key
  std::string cipher_key = args.get("--cipher");

  // Validity Times
  std::string not_before, not_after;
  time_t not_before_time, not_after_time;
  if (args.get("--from", not_before)) {
    if (not_before.size() != 14) throw CertificateException("invalid validity from time " + not_before);
    for (char c : not_before)
      if (c < '0' || c > '9') throw CertificateException("invalid validity from time " + not_before);
    not_before_time = Utility::string_to_time(not_before, "%Y%m%d%H%M%S", true);
  } else {
    not_before_time = std::time(0);
    not_before = Utility::time_to_string(not_before_time, "%Y%m%d%H%M%S", true);
  }
  std::cout << "    Validity Not Before: " << Utility::time_to_string(not_before_time) << " GMT" << std::endl;
  if (args.get("--to", not_after)) {
    if (not_after.size() != 14) throw CertificateException("invalid validity to time " + not_after);
    for (char c : not_after)
      if (c < '0' || c > '9') throw CertificateException("invalid validity to time " + not_after);
    not_after_time = Utility::string_to_time(not_after, "%Y%m%d%H%M%S", true);
  } else {
    struct tm* timeinfo = gmtime(&not_before_time);
    timeinfo->tm_year += 1;
    not_after_time = timegm(timeinfo);
    not_after = Utility::time_to_string(not_after_time, "%Y%m%d%H%M%S", true);
  }
  std::cout << "     Validity Not After: " << Utility::time_to_string(not_after_time) << " GMT" << std::endl;

  // Certificate Signin Request File
  std::string csr_path(args.get("--csr"));
  if (csr_path.size() == 0) csr_path = "request.csr";
  std::ifstream csr_file(csr_path);
  if (!csr_file.good()) {
    throw CertificateException("invalid csr file " + csr_path);
  }
  std::cout << "    Certificate Request: " << csr_path << std::endl;

  // Hash
  std::string hash(args.get("--hash"));
  if (hash.size() == 0) hash = "SHA-1";
  std::cout << "         Hash Algorithm: " << hash << std::endl;

  // Encoding
  std::string encoding = args.get("--encoding");
  if (encoding.size() == 0) encoding = "DER";
  std::cout << "     Encoding Algorithm: " << encoding << std::endl;

  // Certificate File
  std::string certificate_path(args.get("--certificate"));
  if (certificate_path.size() == 0) certificate_path = "certificate.crt";
  std::ofstream certificate_file(certificate_path, std::ios::trunc);
  if (!certificate_file.good()) {
    throw CertificateException("invalid certificate file " + certificate_path);
  }
  std::cout << "            Certificate: " << certificate_path << std::endl;

  // Issuing Certificate
  std::cout << std::endl;
  std::cout << "Issuing Certificate ..." << std::endl;
  bufferstream issuer_stream, pik_stream, csr_stream, crt_stream;
  load_stream(issuer_file, issuer_stream);
  load_stream(issuer_key_file, pik_stream);
  load_stream(csr_file, csr_stream);
  X509::issue(serial_number, issuer_stream, pik_stream, cipher_key, not_before_time, not_after_time, csr_stream, hash, encoding, crt_stream);
  dump_stream(crt_stream, certificate_file);
  certificate_file.close();
  std::cout << "Certificated Issued" << std::endl;
}

void pikval(int argc, char* argv[])
{
  std::cout << "Certificate Validating Arguments: " << std::endl;

  Argument args(argc, argv);

  // Issuer Private Key File
  std::string issuer_key_path;
  if (!args.get("--issuer-pik", issuer_key_path)) {
    issuer_key_path = shipped_certificate("dont-use-private.key");
  }
  std::ifstream issuer_key_file(issuer_key_path);
  if (!issuer_key_file.good()) {
    throw CertificateException("invalid issuer private key file " + issuer_key_path);
  }
  std::cout << "     Issuer Private Key: " << issuer_key_path << std::endl;

  // Encryption Cipher Key
  std::string cipher_key = args.get("--cipher");

  // Certificate File
  std::string certificate_path(args.get("--certificate"));
  if (certificate_path.size() == 0) certificate_path = "certificate.crt";
  std::ifstream certificate_file(certificate_path);
  if (!certificate_file.good()) {
    throw CertificateException("invalid certificate file " + certificate_path);
  }
  std::cout << "            Certificate: " << certificate_path << std::endl;

  // Validating Certificate
  std::cout << std::endl;
  std::cout << "Validating Certificate ..." << std::endl;
  bufferstream pik_stream, crt_stream;
  load_stream(issuer_key_file, pik_stream);
  load_stream(certificate_file, crt_stream);
  X509::validate_by_pik(pik_stream, cipher_key, crt_stream);
  std::cout << "Certificate Validated" << std::endl;  
}

void pukval(int argc, char* argv[])
{
  std::cout << "Certificate Validating Arguments: " << std::endl;

  Argument args(argc, argv);

  // Issuer Public Key File
  std::string issuer_key_path;
  if (!args.get("--issuer-puk", issuer_key_path)) {
    issuer_key_path = shipped_certificate("dont-use-public.key");
  }
  std::ifstream issuer_key_file(issuer_key_path);
  if (!issuer_key_file.good()) {
    throw CertificateException("invalid issuer public key file " + issuer_key_path);
  }
  std::cout << "      Issuer Public Key: " << issuer_key_path << std::endl;

  // Certificate File
  std::string certificate_path(args.get("--certificate"));
  if (certificate_path.size() == 0) certificate_path = "certificate.crt";
  std::ifstream certificate_file(certificate_path);
  if (!certificate_file.good()) {
    throw CertificateException("invalid certificate file " + certificate_path);
  }
  std::cout << "            Certificate: " << certificate_path << std::endl;

  // Validating Certificate
  std::cout << std::endl;
  std::cout << "Validating Certificate ..." << std::endl;
  bufferstream puk_stream, crt_stream;
  load_stream(issuer_key_file, puk_stream);
  load_stream(certificate_file, crt_stream);
  X509::validate_by_puk(puk_stream, crt_stream);
  std::cout << "Certificate Validated" << std::endl;  
}
