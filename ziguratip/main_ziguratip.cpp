#include <cstdlib>
#include <csignal>
#include <clocale>
#include "utility.hpp"
#include "globals.hpp"
#include "argument.hpp"
#include "configuration.hpp"
#include "ziguratipexception.hpp"
#include "shared.cpp"

using namespace Zigurat;

std::string config_path;
std::string locale = "en_US.utf8";
std::string home_path;
std::string catalog_path;
bool        trace_mode = false;
bool        reset_mode = false;
bool        compiler_remote_mode = false;

// Declared here and defined by the loaders below, which are included after the
// shared instance variables they read.
void load_memory(const Configuration&);
void load_library(const Configuration&);
void load_compiler(const Configuration&);
void load_security(const Configuration&);
void load_zigurat(const Configuration&);
void load_zeytun(const Configuration&);

void sigabrt_handler(int code)
{
  if (memory_hexmap_stream != nullptr) memory_hexmap_stream->flush();
  if (memory_data_stream != nullptr) memory_data_stream->flush();
}

namespace
{
  // Every path in the loaders is built as home_path + "data/hexmap", so the
  // separator has to already be there.
  std::string with_trailing_separator(const std::string& path)
  {
    if (path.empty()) return path;
    if (path[path.size() - 1] == '/') return path;
    return path + "/";
  }

  bool as_boolean(const std::string& raw, const std::string& key)
  {
    const std::string value = Utility::to_upper(Utility::trim(raw));
    if (value == "TRUE") return true;
    if (value == "FALSE") return false;
    throw ZiguratIPException("invalid value for '" + key + "'");
  }

  // The server takes one argument and reads everything else from a file, so
  // what this has to describe is mostly the file: where it is looked for, and
  // what the settings that change how it runs are called. Every one of them is
  // documented in place in ziguratip.conf; this is the map, not the manual.
  void help()
  {
    std::cout
      << "\tZiguratIP server -- Zigurat (binary protocol) and Zeytun (HTTP)" << std::endl
      << std::endl
      << "Usage: " << std::endl
      << "\tziguratip [--config=<file>]" << std::endl
      << "\tziguratip --help" << std::endl
      << std::endl
      << "Arguments: " << std::endl
      << "\t--config           ::= \"configuration file\"" << std::endl
      << "\t--help | -h        ::= this" << std::endl
      << std::endl
      << "Configuration: " << std::endl
      << "\tWithout --config, ziguratip.conf is looked for in this order:" << std::endl
      << "\t  1. $ZIGURATIP_HOME/etc/ziguratip.conf" << std::endl
      << "\t  2. ~/ZiguratIP/etc/ziguratip.conf" << std::endl
#if defined(_WIN32) || defined(_WIN64)
      << "\t  3. %PROGRAMDATA%/ZiguratIP/ziguratip.conf" << std::endl
#else
      << "\t  3. /etc/ZiguratIP/ziguratip.conf" << std::endl
#endif
      << "\tIt fails to start rather than guess if none of them is there." << std::endl
      << std::endl
      << "Environment: " << std::endl
      << "\tZIGURATIP_HOME     ::= the runtime home; every path below defaults" << std::endl
      << "\t                       under it, and it is where the configuration" << std::endl
      << "\t                       file is looked for first" << std::endl
#if defined(_WIN32) || defined(_WIN64)
      << "\tPATH               ::= must contain $ZIGURATIP_HOME/lib" << std::endl
#elif defined(__APPLE__)
      << "\tDYLD_LIBRARY_PATH  ::= must contain $ZIGURATIP_HOME/lib" << std::endl
#else
      << "\tLD_LIBRARY_PATH    ::= must contain $ZIGURATIP_HOME/lib" << std::endl
#endif
      << "\tA C++ compiler must be on PATH: Parsi objects are compiled at" << std::endl
      << "\truntime, not only at build time." << std::endl
      << std::endl
      << "Settings, with their defaults: " << std::endl
      << "\t/LOCALE                     en_US.utf8" << std::endl
      << "\t/HOME_PATH                  $ZIGURATIP_HOME" << std::endl
      << "\t/CATALOG_PATH               $HOME_PATH/catalog" << std::endl
      << "\t/LIBRARY_PATH               $HOME_PATH/ld" << std::endl
      << "\t/RESET_MODE                 FALSE  --! TRUE erases every table !--" << std::endl
      << "\t/TRACE_MODE                 TRUE   --! logs every request !--" << std::endl
      << "\t/MEMORY/PAGE_SIZE           8192" << std::endl
      << "\t/TRANSACTION/MODE           NON-AUTOCOMMIT | AUTOCOMMIT" << std::endl
      << "\t/TRANSACTION/ISOLATION_LEVEL READ-COMMITTED --! of five !--" << std::endl
      << "\t/LIBRARY/CACHE_MODE         NONE | GLOBAL | LOCAL" << std::endl
      << "\t/COMPILER/REMOTE_MODE       FALSE  --! clients may compile; code execution !--" << std::endl
      << "\t/COMPILER/CPP               c++" << std::endl
      << "\t/SERVER/TYPE                TCP | IPC" << std::endl
      << "\t/SERVER/PORT                2160   --! the binary protocol !--" << std::endl
      << "\t/SERVER/POOL_SIZE           5      --! concurrent transactions !--" << std::endl
      << "\t/SERVER/TLS_MODE            FALSE" << std::endl
      << "\t/HTTP/PORT                  2190   --! Zeytun !--" << std::endl
      << "\t/HTTP/POOL_SIZE             5      --! concurrent requests !--" << std::endl
      << "\t/HTTP/TLS_MODE              FALSE" << std::endl
      << "\t/HTTP/SESSION_TIMEOUT       1800" << std::endl
      << "\t/SECURITY/CERTIFICATE_PATH  $HOME_PATH/etc/cert" << std::endl
      << "\t/SECURITY/CERTIFICATE       --! this server's own !--" << std::endl
      << "\t/SECURITY/PRIVATE_KEY       --! the key it belongs to !--" << std::endl
      << "\t/SECURITY/AUTHORITY         --! who must have signed a client's !--" << std::endl
      << "\t/SECURITY/PERMISSIONS_MODE  FALSE  --! enforce what a cert grants !--" << std::endl
      << "\t/SECURITY/USERS_PATH        $HOME_PATH/etc/users" << std::endl
      << std::endl
      << "\tEvery setting is documented where it is written, in" << std::endl
      << "\t$ZIGURATIP_HOME/etc/ziguratip.conf. See also doc/configuration.md" << std::endl
      << "\tand doc/security.md." << std::endl;
  }
}

int main(int argc, char** argv)
{
  try {
    Argument args(argc, argv);

    // Before anything is loaded, so asking what the server takes does not
    // require a configuration file to be findable first.
    if (args.flag("--help") || args.flag("-h")) {
      help();
      return 0;
    }

    if (!args.get("--config", config_path))
      config_path = Utility::config_path("ziguratip.conf");

    std::cout << "Configuration path: '" << config_path << "'" << std::endl;
    if (config_path.size() == 0) throw ZiguratIPException("configuration file not found");

    Configuration config(config_path);

    config.get("/LOCALE", locale);
    locale = Utility::trim(locale);
    std::setlocale(LC_ALL, locale.c_str());
    std::cout << "Locale: '" << locale << "'" << std::endl;

    config.get("/HOME_PATH", home_path);
    home_path = Utility::trim(home_path);
    if (home_path.size() == 0) home_path = Utility::env_var("ZIGURATIP_HOME");
    if (home_path.size() == 0) home_path = Utility::user_home() + "/ZiguratIP";
    home_path = with_trailing_separator(home_path);
    std::cout << "Home path: '" << home_path << "'" << std::endl;

    catalog_path = home_path + "catalog";
    if (config.get("/CATALOG_PATH", catalog_path)) catalog_path = Utility::trim(catalog_path);
    std::cout << "Catalog path: '" << catalog_path << "'" << std::endl;

    std::string value = "";

    if (config.get("/TRACE_MODE", value)) trace_mode = as_boolean(value, "/TRACE_MODE");
    Globals::set_trace_mode(trace_mode);
    std::cout << "Trace mode: '" << ((trace_mode) ? "TRUE" : "FALSE") << "'" << std::endl;

    if (args.get("--reset", value) || config.get("/RESET_MODE", value))
      reset_mode = as_boolean(value, "/RESET_MODE");
    Globals::set_reset_mode(reset_mode);
    std::cout << "Reset mode: '" << ((reset_mode) ? "TRUE" : "FALSE") << "'" << std::endl;

    // Handling Abortion
    std::signal(SIGABRT, sigabrt_handler);
    // A client that disappears mid-write must not take the server with it.
    std::signal(SIGPIPE, SIG_IGN);

    // Multiversion Concurrency Control System
    load_memory(config);

    // Library Pool
    load_library(config);

    // Parsi Compiler
    load_compiler(config);

    // Zigurat Binary Server -- runs on its own thread and returns
    load_security(config);

    load_zigurat(config);

    // Zeytun HTTP Server -- blocks, which is what keeps the process alive
    load_zeytun(config);

  } catch (const ZiguratException& error) {
    std::cerr << "ZiguratIP failed to start: " << error.message() << std::endl;
    return 1;
  } catch (const std::exception& error) {
    std::cerr << "ZiguratIP failed to start: " << error.what() << std::endl;
    return 1;
  }

  return 0;
}
