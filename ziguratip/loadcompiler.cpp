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
  std::string cpp_flags = "-Wall -std=c++17 -fPIC";
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

  // Whether the compile function of the binary protocol answers at all.
  //
  // Compiling is not like the other functions. The rest of them read and write
  // rows through the storage engine, which is bounded by what the engine will
  // do; compiling hands a string to a C++ compiler and a linker, runs them, and
  // loads the result into this process. Parsi can embed C++ verbatim, and its
  // INCLUDE and LINK clauses are spliced onto those command lines, so a client
  // permitted to declare one object can run anything the server's user can run.
  // The permission check upstream reads object names, which does not help: the
  // names can be entirely in order while the code behind them is not.
  //
  // So it is off unless somebody turns it on. The parsi program compiles the
  // same sources offline and the server loads the objects it leaves behind,
  // which is the arrangement any exposed instance should be using.
  if (config.get("/COMPILER/REMOTE_MODE", value)) {
    value = Utility::to_upper(Utility::trim(value));
    if (value == "TRUE")
      compiler_remote_mode = true;
    else if (value == "FALSE")
      compiler_remote_mode = false;
    else
      throw ZiguratIPException("invalid value for '/COMPILER/REMOTE_MODE'");
  }
  std::cout << "Compiler remote mode: '" << ((compiler_remote_mode) ? "TRUE" : "FALSE") << "'";
  if (compiler_remote_mode)
    std::cout << "  --! clients may compile; this is arbitrary code execution as this user !--";
  std::cout << std::endl;

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

  // Declaring is the strongest thing a client can do: whoever writes a
  // procedure writes what it reaches. So a declaration takes permission for
  // the object being declared and for everything it requires -- otherwise a
  // caller allowed one schema could compile a procedure that reads another,
  // and calling it would be entirely in order.
  compiler.permission(Globals::require_permission);

  // Both are reached through Globals by the compile function of the binary
  // protocol, and registering them is what makes that pointer non-null. Without
  // this the first Connector::compile dereferenced null and took the server down
  // with it -- the client saw only the acknowledgement sent before it died.
  Globals::set_parser(&parser);
  Globals::set_compiler(&compiler);
}
