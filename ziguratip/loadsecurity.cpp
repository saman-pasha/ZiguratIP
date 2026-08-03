#include "ziguratipexception.hpp"
#include "configuration.hpp"
#include "utility.hpp"
#include "tls.hpp"
#include <fstream>
#include <iostream>
#include <string>


using namespace Zigurat;

// The certificate material, read once and shared by both servers. Zigurat and
// Zeytun present the same identity and trust the same authority: they are two
// doors into one database, not two systems.
std::string              certificate_path;
TLS::HandshakeParameters security_params;
bool                     security_ready = false;


// A file named in the configuration. An absolute path is taken as given;
// anything else is looked for under the certificate directory, so the common
// case reads "CERTIFICATE: server.crt" and moving the directory moves all of it.
static std::string security_file(const std::string& name)
{
  if (name.empty()) return "";
  if (name[0] == '/') return name;
  return certificate_path + name;
}

static void require(const std::string& path, const char* what)
{
  if (path.empty())
    throw ZiguratIPException(std::string("TLS is on but no ") + what + " is configured");

  std::ifstream probe(path);
  if (!probe.good())
    throw ZiguratIPException(std::string("cannot read the ") + what + " " + path);
}


void load_security(const Configuration& conf)
{
  certificate_path = Utility::config_path("cert/");
  if (certificate_path.empty()) {
    std::string home = Utility::env_var("ZIGURATIP_HOME");
    if (!home.empty()) {
      if (home.back() != '/') home.push_back('/');
      certificate_path = home + "etc/cert/";
    }
  }

  std::string configured;
  if (conf.get("/SECURITY/CERTIFICATE_PATH", configured)) {
    certificate_path = Utility::trim(configured);
    if (!certificate_path.empty() && certificate_path.back() != '/')
      certificate_path.push_back('/');
  }

  std::cout << "Certificate path: '" << certificate_path << "'" << std::endl;

  std::string value;
  if (conf.get("/SECURITY/CERTIFICATE", value))
    security_params.credentials.certificate = security_file(Utility::trim(value));
  if (conf.get("/SECURITY/PRIVATE_KEY", value))
    security_params.credentials.private_key = security_file(Utility::trim(value));
  if (conf.get("/SECURITY/PRIVATE_KEY_CIPHER", value))
    security_params.credentials.private_key_cipher = value;
  if (conf.get("/SECURITY/AUTHORITY", value))
    security_params.credentials.authority = security_file(Utility::trim(value));

  security_params.protocol_version = TLS::VERSION_1_2;
  security_params.cipher_suites.push_back(TLS::TLS_RSA_WITH_AES_256_CBC_SHA256);
  security_params.cipher_suites.push_back(TLS::TLS_RSA_WITH_AES_128_CBC_SHA256);
  security_params.compression_methods.push_back(TLS::CompressionMethod::NONE);

  security_ready = !security_params.credentials.certificate.empty()
    && !security_params.credentials.private_key.empty()
    && !security_params.credentials.authority.empty();
}


// Called by whichever server has TLS_MODE on. Refusing to start is the point: a
// server told to be secure that quietly fell back to plaintext would be worse
// than one that never offered it, because nobody would notice.
void require_security(const char* who)
{
  if (!security_ready)
    throw ZiguratIPException(std::string(who) + " has TLS_MODE on, but SECURITY/CERTIFICATE,"
			     " SECURITY/PRIVATE_KEY and SECURITY/AUTHORITY are not all set");

  require(security_params.credentials.certificate, "certificate");
  require(security_params.credentials.private_key, "private key");
  require(security_params.credentials.authority,   "authority certificate");

  std::cout << who << " certificate: '" << security_params.credentials.certificate << "'" << std::endl;
  std::cout << who << " authority: '" << security_params.credentials.authority << "'" << std::endl;
}
