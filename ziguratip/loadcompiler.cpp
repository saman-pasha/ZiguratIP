#include "ziguratipexception.hpp"
#include "configuration.hpp"
#include "globals.hpp"
#include "utility.hpp"
#include "token.hpp"
#include "tokenizer.hpp"
#include "shared.cpp"


using namespace Zigurat;

Parser   parser;
Compiler compiler;

void load_compiler(const Configuration &config)
{
  std::string cpp = "c++";
  std::string cpp_flags = "-Wall -std=c++11 -fPIC";
  std::string ld_flags = "-shared";

  config.get("/COMPILER/CPP", cpp);
  config.get("/COMPILER/CPP_FLAGS", cpp_flags);
  config.get("/COMPILER/LD_FLAGS", ld_flags);

  std::string include_path = home_path + "include";
  std::string obj_path = home_path + "obj";
  std::string lib_path = home_path + "lib";
  std::string tmp_path = home_path + "tmp";
  std::string ld_path = home_path + "ld";

  config.get("/COMPILER/INCLUDE_PATH", include_path);
  config.get("/COMPILER/OBJ_PATH", obj_path);
  config.get("/COMPILER/LIB_PATH", lib_path);
  config.get("/COMPILER/TMP_PATH", tmp_path);
  config.get("/COMPILER/LD_PATH", ld_path);

  std::cout << "Compiler cpp: '" << cpp << "'" << std::endl;
  std::cout << "Compiler cpp flags: '" << cpp_flags << "'" << std::endl;
  std::cout << "Compiler ld flags: '" << ld_flags << "'" << std::endl;
  std::cout << "Compiler include path: '" << include_path << "'" << std::endl;
  std::cout << "Compiler obj path: '" << obj_path << "'" << std::endl;
  std::cout << "Compiler lib path: '" << lib_path << "'" << std::endl;
  std::cout << "Compiler tmp path: '" << tmp_path << "'" << std::endl;
  std::cout << "Compiler ld path: '" << ld_path << "'" << std::endl;

  // The shipped file is etc/patterns.conf; look for it the same way the other
  // configuration files are found.
  std::string patterns_file = Utility::config_path("patterns.conf");
  if (patterns_file.size() == 0) patterns_file = home_path + "etc/patterns.conf";
  config.get("/PARSER/PATTERNS_FILE", patterns_file);
  std::cout << "Parser patterns file: '" << patterns_file << "'" << std::endl;

  std::string value = "";

  bool parser_trace = false;
  if (config.get("/PARSER/TRACE_MODE", value)) {
    value = Utility::to_upper(value);
    if (value == "TRUE")
      parser_trace = true;
    else if (value == "FALSE")
      parser_trace = false;
    else
      throw ZiguratIPException("invalid value for '/PARSER/TRACE_MODE'");
  }
  std::cout << "Parser trace mode: '" << ((parser_trace) ? "TRUE" : "FALSE") << "'" << std::endl;

  parser.configure(patterns_file, parser_trace);
  compiler.configure(cpp, cpp_flags, ld_flags, catalog_path, include_path, obj_path, lib_path, tmp_path, ld_path, trace_mode);

  // Both are reached through Globals by the compile function of the binary
  // protocol, and registering them is what makes that pointer non-null. Without
  // this the first Connector::compile dereferenced null and took the server down
  // with it -- the client saw only the acknowledgement sent before it died.
  Globals::set_parser(&parser);
  Globals::set_compiler(&compiler);
}
