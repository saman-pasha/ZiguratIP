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
  memory_hexmap_file.flush();
  memory_data_file.flush();
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
}

int main(int argc, char** argv)
{
  try {
    Argument args(argc, argv);

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
