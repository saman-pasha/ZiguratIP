#include "ziguratipexception.hpp"
#include "configuration.hpp"
#include "globals.hpp"
#include "utility.hpp"
#include "x509.hpp"
#include "tls.hpp"
#include <fstream>
#include <iostream>
#include <string>


using namespace Zigurat;

// The certificate material, read once and shared by both servers. Zigurat and
// Zeytun present the same identity and trust the same authority: they are two
// doors into one database, not two systems.
std::string              certificate_path;
std::string              users_path;
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


// Is this subject allowed to open a connection at all? The answer is a file in
// the users directory named after the subject, and nothing else: no database,
// no table, no state that a restore could quietly bring back. Deleting the file
// ends every session that subject could start, whichever of its certificates it
// holds.
bool user_registered(const std::string& subject)
{
  if (subject.empty()) return false;
  if (users_path.empty()) return false;

  return Utility::file_exists(users_path + X509::subject_file_name(subject));
}


static std::string configured_path(const Configuration& conf, const char* key, const std::string& fallback)
{
  std::string path = fallback;

  std::string configured;
  if (conf.get(key, configured)) path = Utility::trim(configured);

  if (!path.empty() && path.back() != '/') path.push_back('/');
  return path;
}


void load_security(const Configuration& conf)
{
  std::string home = Utility::env_var("ZIGURATIP_HOME");
  if (!home.empty() && home.back() != '/') home.push_back('/');

  certificate_path = Utility::config_path("cert/");
  if (certificate_path.empty() && !home.empty()) certificate_path = home + "etc/cert/";
  certificate_path = configured_path(conf, "/SECURITY/CERTIFICATE_PATH", certificate_path);

  users_path = Utility::config_path("users/");
  if (users_path.empty() && !home.empty()) users_path = home + "etc/users/";
  users_path = configured_path(conf, "/SECURITY/USERS_PATH", users_path);

  std::string value;
  bool permissions_mode = false;
  if (conf.get("/SECURITY/PERMISSIONS_MODE", value)) {
    value = Utility::to_upper(Utility::trim(value));
    if (value == "TRUE")
      permissions_mode = true;
    else if (value == "FALSE")
      permissions_mode = false;
    else
      throw ZiguratIPException("invalid value for '/SECURITY/PERMISSIONS_MODE'");
  }
  Globals::set_permissions_mode(permissions_mode);

  std::cout << "Certificate path: '" << certificate_path << "'" << std::endl;
  std::cout << "Users path: '" << users_path << "'" << std::endl;
  std::cout << "Permissions mode: '" << ((permissions_mode) ? "TRUE" : "FALSE") << "'" << std::endl;

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

  // The authority's signature says a peer is genuine. The users directory says
  // it is still welcome. Both have to hold, and this is where the second one is
  // asked, before a handshake finishes and before any protocol is spoken.
  //
  // With permissions off there is no register to consult: holding a certificate
  // the authority issued is the whole of the admission test, exactly as it was
  // before any of this existed.
  if (permissions_mode) security_params.authorize = user_registered;

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

  if (!Globals::permissions_mode()) {
    std::cout << who << ": permissions are off, so any holder of a certificate this"
	      << " authority issued may connect and reach everything" << std::endl;
    return;
  }

  // An unwritable or missing users directory would refuse everybody, which is
  // safe but looks exactly like a configuration mistake. Say so at startup
  // rather than once per rejected connection.
  if (!Utility::make_directory(users_path))
    throw ZiguratIPException("cannot use the users directory " + users_path);

  const size_t registered = Utility::directory_files(users_path).size();
  std::cout << who << " registered users: " << registered << std::endl;

  if (registered == 0)
    std::cout << who << ": no user is registered in " << users_path
	      << ", so every connection will be refused; register one with \"ca put\"" << std::endl;
}
