#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <ctime>
#include <clocale>
#include "zexception.hpp"
#include "utility.hpp"
#include "configuration.hpp"
#include "argument.hpp"
#include "tokenizer.hpp"
#include "parser.hpp"
#include "expression.hpp"
#include "compiler.hpp"
#include "globals.hpp"
#include "parsiexception.hpp"


using namespace Zigurat;

#define LOCALE "en_US.utf8"

std::string locale;
std::string home_path;
std::string conf_path;
std::string catalog_path;
std::string library_path;

namespace
{
  // parsi is the server's compiler with no server around it, so it reads the
  // server's configuration file and the same keys out of it. What it does not
  // read is anything about ports or storage: it compiles, and stops.
  void help()
  {
    std::cout
      << "\tZiguratIP Parsi compiler" << std::endl
      << std::endl
      << "Usage: " << std::endl
      << "\tparsi <file.parsi> [--config=<file>]" << std::endl
      << "\tparsi --help" << std::endl
      << std::endl
      << "Arguments: " << std::endl
      << "\t<file.parsi>       ::= the source to compile; one file, any number" << std::endl
      << "\t                       of objects, compiled in the order written" << std::endl
      << "\t--config           ::= \"configuration file\"" << std::endl
      << "\t--help | -h        ::= this" << std::endl
      << std::endl
      << "What it produces, for each object in the file: " << std::endl
      << "\t$LD_PATH/lib_NAME_.so       the loadable object" << std::endl
      << "\t$LD_PATH/_NAME_.hpp         its generated header" << std::endl
      << "\t$CATALOG_PATH/_NAME_.conf   its catalogue entry" << std::endl
      << "\t$TMP_PATH/_NAME_.cpp|.out   the generated C++ and the build log" << std::endl
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
      << std::endl
      << "Settings it reads, with their defaults: " << std::endl
      << "\t/LOCALE                     en_US.utf8" << std::endl
      << "\t/HOME_PATH                  $ZIGURATIP_HOME" << std::endl
      << "\t/CATALOG_PATH               $HOME_PATH/catalog" << std::endl
      << "\t/LIBRARY_PATH               $HOME_PATH/ld" << std::endl
      << "\t/PARSER/PATTERNS_FILE       $HOME_PATH/etc/patterns.conf" << std::endl
      << "\t/PARSER/TRACE_MODE          FALSE  --! prints the parse !--" << std::endl
      << "\t/COMPILER/CPP               c++    --! must be on PATH !--" << std::endl
      << "\t/COMPILER/CPP_FLAGS         -Wall -std=c++11 -fPIC" << std::endl
      << "\t/COMPILER/LD_FLAGS          -shared" << std::endl
      << "\t/COMPILER/INCLUDE_PATH      $HOME_PATH/include" << std::endl
      << "\t/COMPILER/OBJ_PATH          $HOME_PATH/obj" << std::endl
      << "\t/COMPILER/LIB_PATH          $HOME_PATH/lib" << std::endl
      << "\t/COMPILER/TMP_PATH          $HOME_PATH/tmp" << std::endl
      << "\t/COMPILER/LD_PATH           $HOME_PATH/ld" << std::endl
      << "\t/COMPILER/TRACE_MODE        FALSE  --! prints the generated C++ !--" << std::endl
      << std::endl
      << "\tAn object named in REQUIRES has to be compiled already, so order" << std::endl
      << "\tmatters. See doc/parsi.md and doc/configuration.md." << std::endl;
  }
}

namespace
{
  // The file being compiled, so the catch in main can name it.  It is set once
  // the argument is known and stays empty until then -- a failure before that
  // is about the configuration, not about any source.
  std::string compiling;
}

static int run(int argc, char* argv[])
{
  clock_t begin_time = clock();

  Argument args(argc, argv);

  // Before the configuration file is looked for, so asking what parsi takes
  // works in a tree that is not set up yet.
  if (args.flag("--help") || args.flag("-h") || argc == 1) {
    help();
    return 0;
  }

  if (!args.get("--config", conf_path)) {
    conf_path = Utility::config_path("ziguratip.conf");
  }
  
  if (conf_path.size() > 0) {
    std::ifstream conf_file(conf_path);
    if (conf_file.good()) {
      std::cout << "Configuration path: '" << conf_path << "'" << std::endl;
    } else {
      std::cout << "Configuration path: '" << conf_path << "' not found" << std::endl;
    }
    conf_file.close();
  } else {
    throw ParsiException("Configuration: not found");
  }

  Configuration conf(conf_path);
  
  locale = LOCALE;
  conf.get("/LOCALE", locale);
  std::setlocale(LC_ALL, locale.c_str());
  std::cout << "Locale: '" << locale << "'" << std::endl;  

  conf.get("/HOME_PATH", home_path);
  if (home_path.size() == 0) {
    // ZIGURATIP_HOME already names the home directory; only the fall back to
    // the user's home needs the ZiguratIP suffix. This matches the server.
    home_path = Utility::env_var("ZIGURATIP_HOME");
    if (home_path.size() == 0) {
      home_path = Utility::user_home();
      if (home_path.size() > 0 && home_path[home_path.size() - 1] != '/')
	home_path.push_back('/');
      home_path = home_path + "ZiguratIP";
    }
    if (home_path.size() > 0 && home_path[home_path.size() - 1] != '/')
      home_path.push_back('/');
  } else {
    if (home_path[home_path.size() - 1] != '/')
      home_path.push_back('/');
  }
  std::cout << "Home path: '" << home_path << "'" << std::endl;

  catalog_path = home_path + "catalog";
  conf.get("/CATALOG_PATH", catalog_path);
  std::cout << "Catalog path: '" << catalog_path << "'" << std::endl;

  library_path = home_path + "ld";
  conf.get("/LIBRARY_PATH", library_path);
  std::cout << "Library path: '" << library_path << "'" << std::endl;

  std::string patterns_path = home_path + "etc/patterns.conf";
  conf.get("/PARSER/PATTERNS_FILE", patterns_path);
  std::cout << "Parser patterns file: '" << patterns_path << "'" << std::endl;

  std::string parser_trace_str;
  bool parser_trace = false;
  if (conf.get("/PARSER/TRACE_MODE", parser_trace_str)) {
    parser_trace_str = Utility::to_upper(parser_trace_str);
    if (parser_trace_str == "TRUE")
      parser_trace = true;
    else if (parser_trace_str == "FALSE")
      parser_trace = false;
    else
      throw ParsiException("invalid value for '/PARSER/TRACE_MODE'");
  }
  std::cout << "Parser trace mode: '" << ((parser_trace) ? "TRUE" : "FALSE" ) << "'" << std::endl;
	
  // The first argument that is not a flag, wherever it sits: "parsi
  // --config=x file.parsi" used to take "--config=x" as the file name and
  // report that it did not exist.
  std::string file_name;
  if (args.get("", file_name)) {
    compiling = file_name;
    std::ifstream file(file_name);
    if (!file.good()) {
      throw ParsiException("file not found");
    }
    std::string line;
    std::stringstream content;
    while (!file.eof()) {
      std::getline(file, line);
      content << line << '\n';
    }

    // Compiler takes its toolchain and paths explicitly now, read from the
    // same configuration keys and defaults load_compiler uses in the server.
    std::string cpp = "c++";
    std::string cpp_flags = "-Wall -std=c++11 -fPIC";
    std::string ld_flags = "-shared";
    conf.get("/COMPILER/CPP", cpp);
    conf.get("/COMPILER/CPP_FLAGS", cpp_flags);
    conf.get("/COMPILER/LD_FLAGS", ld_flags);

    std::string include_path = home_path + "include";
    std::string obj_path = home_path + "obj";
    std::string lib_path = home_path + "lib";
    std::string tmp_path = home_path + "tmp";
    conf.get("/COMPILER/INCLUDE_PATH", include_path);
    conf.get("/COMPILER/OBJ_PATH", obj_path);
    conf.get("/COMPILER/LIB_PATH", lib_path);
    conf.get("/COMPILER/TMP_PATH", tmp_path);
    conf.get("/COMPILER/LD_PATH", library_path);

    std::string compiler_trace_str;
    bool compiler_trace = false;
    if (conf.get("/COMPILER/TRACE_MODE", compiler_trace_str)) {
      compiler_trace_str = Utility::to_upper(Utility::trim(compiler_trace_str));
      if (compiler_trace_str == "TRUE")
	compiler_trace = true;
      else if (compiler_trace_str == "FALSE")
	compiler_trace = false;
      else
	throw ParsiException("invalid value for '/COMPILER/TRACE_MODE'");
    }
    std::cout << "Compiler trace mode: '" << ((compiler_trace) ? "TRUE" : "FALSE") << "'" << std::endl;

    Parser parser(patterns_path, parser_trace);
    Compiler compiler(cpp, cpp_flags, ld_flags, catalog_path,
		      include_path, obj_path, lib_path, tmp_path, library_path,
		      compiler_trace);

    std::list<Token> tokens;
    Tokenizer::tokenize(content.str(), tokens);
    Expression ast = parser.parse("SUITE", tokens);
    compiler.compile(ast);

  } else {
    throw ParsiException("no input file");
  }

  clock_t end_time = clock();

  std::cout << "parsi compilation time : " <<  1000.0  * (end_time - begin_time) / CLOCKS_PER_SEC << " ms" << std::endl;
  return 0;
}

// EVERY THROW USED TO REACH std::terminate.  main had no catch at all, so the
// most ordinary thing that can happen -- a typo in the source -- ended as
//
//     terminate called after throwing an instance of 'Zigurat::ParseException'
//       what():  syntax error at line 4 column 3 near 'RETRN'
//     Aborted
//
// with status 134.  The message was there, but wrapped in a crash: a caller
// cannot tell it from parsi actually falling over, and 134 is what a shell
// reports for a signal.  It is an ordinary failure and exits 1 now, with the
// file named in front of the position so an editor can open it -- the same
// line parsic prints, from the same Utility::diagnostic.
int main(int argc, char* argv[])
{
  try {
    return run(argc, argv);
  } catch (const ZiguratException& e) {
    std::cerr << (compiling.empty() ? std::string("parsi: ") + e.what()
                                    : Utility::diagnostic(compiling, e.what()))
	      << std::endl;
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "parsi: " << e.what() << std::endl;
    return 1;
  }
}
