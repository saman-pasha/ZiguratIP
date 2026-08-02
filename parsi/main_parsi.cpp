#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <ctime>
#include <clocale>
#include "zexception.h"
#include "utility.h"
#include "configuration.h"
#include "argument.h"
#include "tokenizer.h"
#include "parser.h"
#include "expression.h"
#include "compiler.h"
#include "globals.h"
#include "parsiexception.h"


using namespace Zigurat;

#define LOCALE "en_US.utf8"

std::string locale;
std::string home_path;
std::string conf_path;
std::string catalog_path;
std::string library_path;

int main(int argc, char* argv[])
{
  clock_t begin_time = clock();

  Argument args(argc, argv);
    
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
	
  if (argc > 1) {
    std::string file_name = argv[1];
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
