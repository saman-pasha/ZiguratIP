#include "compiler.h"
#include "compileexception.h"
#include "utility.h"
#include "libraryloader.h"
#include <fstream>
#include <sstream>
#include <set>


namespace Zigurat
{

  std::string Compiler::TAB1(1, '\t');
  std::string Compiler::TAB2(2, '\t');
  std::string Compiler::TAB3(3, '\t');
  std::string Compiler::TAB4(4, '\t');
  std::string Compiler::TAB5(5, '\t');

  Compiler::Compiler()
    : _expr(*this, { })
  {
    std::stringstream ss_endl;
    ss_endl << std::endl;
    this->ENDL = ss_endl.str();
    this->ENDL_LENGTH = ss_endl.tellp();
  }
  
  // The generated paths are built as directory + name, so the separator has to
  // be part of the directory. Every caller passes them without one.
  static std::string with_separator(const std::string& path)
  {
    if (path.empty()) return path;
    if (path[path.size() - 1] == '/') return path;
#if defined(_WIN32) || defined(_WIN64)
    if (path[path.size() - 1] == '\\') return path;
#endif
    return path + "/";
  }

  Compiler::Compiler(std::string cpp, std::string cpp_flags, std::string ld_flags,
		     std::string catalog_path, std::string include_path, std::string obj_path, 
		     std::string lib_path, std::string tmp_path, std::string ld_path, bool trace_mode)
    : _expr(*this, { }), _cpp(cpp), _cpp_flags(cpp_flags), _ld_flags(ld_flags),
      _catalog_path(with_separator(catalog_path)), _include_path(with_separator(include_path)),
      _obj_path(with_separator(obj_path)), _lib_path(with_separator(lib_path)),
      _tmp_path(with_separator(tmp_path)), _ld_path(with_separator(ld_path)),
      _trace_mode(trace_mode)
  {
    std::stringstream ss_endl;
    ss_endl << std::endl;
    this->ENDL = ss_endl.str();
    this->ENDL_LENGTH = ss_endl.tellp();
  }

  void Compiler::configure(std::string cpp, std::string cpp_flags, std::string ld_flags,
			   std::string catalog_path, std::string include_path, std::string obj_path, 
			   std::string lib_path, std::string tmp_path, std::string ld_path, bool trace_mode)
  {
    this->_cpp = cpp;
    this->_cpp_flags = cpp_flags;
    this->_ld_flags = ld_flags;
    this->_catalog_path = with_separator(catalog_path);
    this->_include_path = with_separator(include_path);
    this->_obj_path = with_separator(obj_path);
    this->_lib_path = with_separator(lib_path);
    this->_tmp_path = with_separator(tmp_path);
    this->_ld_path = with_separator(ld_path);
    this->_trace_mode = trace_mode;
  }

  void Compiler::compile(const Expression& ast)
  {
    this->_includes.clear();
    this->_links.clear();
    if (ast.token.value == "SUITE") {
      this->_suite(ast);
    } else {
      throw CompileException("wrong parse", ast);
    }
  }

  void Compiler::_build(std::string name, std::list<std::string>& requires, 
			std::stringstream& head, std::stringstream& impl, std::stringstream& conf, const Expression& expr)
  {
    std::string head_file_path = this->_ld_path + name + ".h";
    std::string impl_file_path = this->_tmp_path + name + ".cpp";
    std::string conf_file_path = this->_catalog_path + name + ".conf";
    std::string obj_file_path = this->_obj_path + name + ".o";
    std::string out_file_path = this->_tmp_path + name + ".out";

#if defined(_WIN32) || defined(_WIN64)
    std::string ld_file_path = "\"" + this->_ld_path + name + ".dll\"";
#else
    std::string ld_file_path = this->_ld_path + "lib" + name + ".so";
#endif

    std::ofstream head_file(head_file_path, std::ios::trunc);
    std::ofstream impl_file(impl_file_path, std::ios::trunc);
    std::ofstream conf_file(conf_file_path, std::ios::trunc);

    for (std::string& header : this->_includes) {
      head_file << "#include " << header.substr(11, header.size() - 21) << std::endl;
    }

    head_file << head.str();
    impl_file << impl.str();
    conf_file << conf.str();
    
    head_file.flush();
    impl_file.flush();
    conf_file.flush();

    head_file << "extern \"C\" std::vector<std::string> links();" << std::endl;
    impl_file << "extern \"C\" std::vector<std::string> links() { return {";

    std::set<std::string> libs_pack;
    std::stringstream libs_pack_code;
    for (std::string& lib : this->_libs) {
      libs_pack.insert("-l" + lib);
    }
    
    bool has_link = false;
    for (std::string& lib : this->_links) {
      libs_pack.insert(lib.substr(11, lib.size() - 21));
      impl_file << "\"" << lib.substr(11, lib.size() - 21) << "\", ";
      has_link = true;
    }

    for (std::string& lib : requires) {
#if defined(_WIN32) || defined(_WIN64)
      auto ldhandle = LibraryLoader::handle("\"" + this->_ld_path + lib + ".dll\"");
#else
      auto ldhandle = LibraryLoader::handle(this->_ld_path + "lib" + lib + ".so");
#endif
      auto symlinks = (Compiler::links_t)LibraryLoader::symbol(ldhandle, "links");
      std::vector<std::string> lib_links = symlinks();
      impl_file << "\"-l" << lib << "\", ";
      for (std::string& lib_link : lib_links) {
	libs_pack.insert(lib_link);
        impl_file << "\"" << lib_link << "\", ";
      }
      libs_pack.insert("-l" + lib);

      LibraryLoader::close(ldhandle);
      has_link = true;
    }

    for (const std::string& lib : libs_pack) {
      libs_pack_code << lib << " ";
    }

    if (has_link)
      impl_file.seekp(-2, std::ios::cur);
    impl_file << "}; }" << std::endl;

    head_file.close();
    impl_file.close();
    conf_file.close();

    if (this->_trace_mode) {
      std::cout << head.str() << std::endl;
      std::cout << impl.str() << std::endl;
    }

    std::string cpp_compile_cmd = this->_cpp + " " + this->_cpp_flags + " -I" + this->_ld_path
      + " -I" + this->_include_path + " -c " + impl_file_path + " -o " + obj_file_path;
    std::string echo_compile_cmd = "echo \"" + cpp_compile_cmd + "\" > " + out_file_path + ENDL;
    system(echo_compile_cmd.c_str());
    cpp_compile_cmd += " >> " + out_file_path + " 2>&1";

    std::string cpp_build_cmd = this->_cpp + " " + this->_ld_flags + " -L" + this->_lib_path + " -L" + this->_ld_path 
      + " -o " + ld_file_path + " " + obj_file_path + " " + libs_pack_code.str();
    std::string echo_build_cmd = "echo \"" + cpp_build_cmd + "\" >> " + out_file_path + ENDL;
    system(echo_build_cmd.c_str());
    cpp_build_cmd += " >> " + out_file_path + " 2>&1";

    if (this->_trace_mode) {
      std::cout << cpp_compile_cmd << std::endl;
    }

    int cpp_compile_err = system(cpp_compile_cmd.c_str());

    if (cpp_compile_err > 0) {
      std::ifstream out_file(out_file_path);
      std::stringstream out_ss;
      out_ss << out_file.rdbuf();
      std::string error_str = out_ss.str();
      throw CompileException(Utility::split(error_str.substr(error_str.find("error:")), '\n')[0], expr);
    }
    
    if (this->_trace_mode) {
      std::cout << cpp_build_cmd << std::endl;
    }

    int cpp_build_err = system(cpp_build_cmd.c_str());

    if (cpp_build_err > 0) {
      std::ifstream out_file(out_file_path);
      std::stringstream out_ss;
      out_ss << out_file.rdbuf();
      std::string error_str = out_ss.str();
      throw CompileException(Utility::split(error_str.substr(error_str.find("error:")), '\n')[0], expr);
    }
  }

  void Compiler::_clause(const Expression& ast, std::stringstream& code, int lvl)
  {
    if (ast.token.value == "ECHO") {
      this->_echo(ast, code, lvl);
    } else if (ast.token.value == "DECLARE") {
      this->_declare(ast, code, lvl, false, false, "");
    } else if (ast.token.value == "SET") {
      this->_set(ast, code, lvl);
    } else if (ast.token.value == "CALL") {
      this->_call(ast, code, lvl);
    } else if (ast.token.value == "RETURN") {
      this->_return(ast, code, lvl);
    } else if (ast.token.value == "IF") {
      this->_if(ast, code, lvl);
    } else if (ast.token.value == "DO") {
      this->_do(ast, code, lvl);
    } else if (ast.token.value == "WHILE") {
      this->_while(ast, code, lvl);
    } else if (ast.token.value == "CONTINUE") {
      this->_continue(ast, code, lvl);
    } else if (ast.token.value == "BREAK") {
      this->_break(ast, code, lvl);
    } else if (ast.token.value == "TRANSACTION") {
      this->_transaction(ast, code, lvl);
    } else if (ast.token.value == "TRY") {
      this->_try(ast, code, lvl);
    } else if (ast.token.value == "THROW") {
      this->_throw(ast, code, lvl);
    } else if (ast.token.value == "INSERT") {
      this->_insert(ast, code, lvl);
    } else if (ast.token.value == "SELECT") {
      this->_select(ast, code, lvl);
    } else if (ast.token.value == "UPDATE") {
      this->_update(ast, code, lvl);
    } else if (ast.token.value == "DELETE") {
      this->_delete(ast, code, lvl);
    } else {
      throw CompileException("unknown clause", ast);
    }
  }
	
  void Compiler::_suite(const Expression& ast)
  {
    for (const Expression& suite : ast.args) {
      if (suite.token.value == "INCLUDE") {
	this->_include(suite);
      } else if (suite.token.value == "LINK") {
	this->_link(suite);
      } else if (suite.token.value == "TABLE") {
	this->_table(suite);
      } else if (suite.token.value == "PROCEDURE") {
	this->_procedure(suite);
      } else if (suite.token.value == "CLASS") {
	this->_class(suite, false);
      } else if (suite.token.value == "PAGE") {
	this->_class(suite, true);
      } else if (suite.token.value == "TYPE") {
	this->_type(suite);
      } else if (suite.token.value == "ENUM") {
	this->_enum(suite);
      } else if (suite.token.value == "SEQUENCE") {
	this->_sequence(suite);
      } else {
	throw CompileException("wrong suite", suite);
      }
    }
  }

  std::string Compiler::_name(const Expression& ast) const
  {
    const Expression* expr = &ast;
    std::string name = expr->token.value;
    while (!expr->args.empty() && expr->args[0].token.value != "$tmpl") {
      expr = &(expr->args[0]);
    }
    return expr->token.value;
  } 

  std::string Compiler::_domain(const Expression& ast) const
  {
    const Expression* expr = &ast;
    std::string domain;
    while (!expr->args.empty()) {
      domain += expr->token.value + "::";
      expr = &(expr->args[0]);
    }
    if (!domain.empty())
      return domain.substr(0, domain.size() - 2);
    return domain;
  }

  std::string Compiler::_full_name(const Expression& ast) const
  {
    const Expression* expr = &ast;
    std::string name = expr->token.value;
    while (!expr->args.empty()) {
      expr = &(expr->args[0]);
      name = name + "_" + expr->token.value;
    }
    return name;
  } 

  std::string Compiler::_type_name(const Expression& ast) const
  {
    const Expression* expr = &ast;
    std::string name = expr->token.value;
    while (!expr->args.empty()) {
      if (expr->args[0].token.value == "$tmpl") {
	name = name + "< ";
	for (const Expression& ch_expr : expr->args[0].args) {
	  name = name + this->_type_name(ch_expr) + ", ";
	}
	name = name.substr(0, name.size() - 2) + " >";
	if (expr->args.size() > 1) {
	  expr = &(expr->args[1]);
	} else {
	  return name;
	}
      } else {
	expr = &(expr->args[0]);
      }
      name = name + "::" + expr->token.value;
    }
    return name;
  } 

  std::string Compiler::_guard_name(const Expression& ast) const
  {
    const Expression* expr = &ast;
    std::string name = "_" + expr->token.value;
    while (!expr->args.empty() && expr->args[0].token.value != "$tmpl") {
      expr = &(expr->args[0]);
      name = name + "_" + expr->token.value;
    }
    return name + "_H_";
  }
	
  std::string Compiler::_include_name(const Expression& ast) const
  {
    const Expression* expr = &ast;
    std::string name = "_" + expr->token.value;
    while (!expr->args.empty() && expr->args[0].token.value != "$tmpl") {
      expr = &(expr->args[0]);
      name = name + "::" + expr->token.value;
    }
    return name + "_";
  }
	
  void Compiler::_open_namespace(const Expression& ast, std::stringstream& code)
  {
    if (ast.args.empty() || ast.args[0].token.value == "$tmpl") {
      //      code << "namespace { " << std::endl;
    } else {
      const Expression* expr = &ast;
      while (!expr->args.empty() && expr->args[0].token.value != "$tmpl") {
	code << "namespace " << expr->token.value << " { ";
	expr = &(expr->args[0]);
      }
      code << std::endl;
    }
  }

  void Compiler::_close_namespace(const Expression& ast, std::stringstream& code)
  {
    if (ast.args.empty() || ast.args[0].token.value == "$tmpl") {
      //      code << "} " << std::endl;
    } else {
      const Expression* expr = &ast;
      while (!expr->args.empty() && expr->args[0].token.value != "$tmpl") {
	code << "} ";
	expr = &(expr->args[0]);
      }
      code << std::endl;
    }
  }

  void Compiler::_echo(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    for (const Expression& expr : ast.args) {
      code << tab << "*Globals::echo_stream() << ";
      this->_expr.compile(expr, code);
      code << ';' << std::endl;
    }
  }

  void Compiler::_declare(const Expression& ast, std::stringstream& code, int lvl, bool in_head, bool in_impl, std::string class_name)
  {
    std::string tab(lvl, '\t');
    for (const Expression& expr : ast.args) {
      size_t offset = 0;
      char storage = 'a';
      if (expr.token.value == "GLOBAL") {
        storage = 's';
	offset++;
      } else if (expr.token.value == "SESSION") {
        storage = 't';
	offset++;
      } else if (in_impl) {
	continue;
      }
      const Expression* expr_ptr = (offset == 1) ? &expr.args[0] : &expr;
      code << tab;
      
      //if (storage == 't' && (in_head || in_impl))
      //throw CompileException("session local as class member", expr);

      if (!in_head && !in_impl) {
	if (storage == 's') {
	  code << "static ";
	} else if (storage == 't') {
	  code << "static thread_local ";
	}
      } else if (in_head && !in_impl) {
	if (storage == 's') {
	  code << "static ";
	} else if (storage == 't') {
	  code << "static thread_local ";
	}
      } else if (!in_head && in_impl) {
	if (storage == 't') {
	  code << "thread_local ";
	}
      }
      code << this->_type_name(expr_ptr->args[0]) << ((in_impl) ? " " + class_name + "::" : " ") << (expr_ptr->token.value);
      if (!in_head && expr_ptr->args.size() > 1) {
	if (expr_ptr->args[1].token.value == "=") {
	  code << " = ";
	  this->_expr.compile(expr_ptr->args[2], code);
	} else {
	  code << '(';
	  for (size_t i = 1; i < expr_ptr->args.size(); i++) {
	    this->_expr.compile(expr_ptr->args[i], code);
	    code << ", ";
	  }
	  code.seekp(-2, std::ios::cur);
	  code << ')';
	}
      }
      code << ';' << std::endl;
    }
  }

  void Compiler::_set(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab;
    this->_expr.compile(ast.args[0], code);
    code << " = ";
    this->_expr.compile(ast.args[1], code);
    code << ';' << std::endl;
  }

  void Compiler::_call(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    if (ast.args.size() == 1) {
      code << tab;
      this->_expr.compile(ast.args[0], code);
    } else {
      code << tab << ast.args[0].token.value << " = ";
      this->_expr.compile(ast.args[1], code);
    }
    code << ';' << std::endl;
  }

  void Compiler::_return(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab << "return";
    if (ast.args.size() > 0) {
      code << ' ';
      this->_expr.compile(ast.args[0], code);
    }
    code << ';' << std::endl;
  }
 
  void Compiler::_if(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab << "if (";
    this->_expr.compile(ast.args[0], code);
    code << ") {" << std::endl;
    for (const Expression& expr : ast.args[1].args) {
      this->_clause(expr, code, lvl + 1);
    }
    for (size_t i = 2; i < ast.args.size(); i++) {
      if (ast.args[i].args.size() > 1) {
	code << tab << "} else if (";
	this->_expr.compile(ast.args[i].args[0], code);
	code << ") {" << std::endl;
	for (const Expression& expr : ast.args[i].args[1].args) {
	  this->_clause(expr, code, lvl + 1);
	}    
      } else {
	code << tab << "} else {" << std::endl;
	for (const Expression& expr : ast.args[i].args[0].args) {
	  this->_clause(expr, code, lvl + 1);
	}
      }
    }
    code << tab << "}" << std::endl;    
  }

  void Compiler::_do(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab << "do {";
    for (const Expression& expr : ast.args[0].args) {
      this->_clause(expr, code, lvl + 1);
    }
    code << tab << "} while (" << std::endl;
    this->_expr.compile(ast.args[1], code);
    code << ");" << std::endl;
  }

  void Compiler::_while(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab << "while (";
    this->_expr.compile(ast.args[0], code);
    code << ") {" << std::endl;
    for (const Expression& expr : ast.args[1].args) {
      this->_clause(expr, code, lvl + 1);
    }
    code << tab << "}" << std::endl;
  }

  void Compiler::_continue(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab << "continue;" << std::endl;
  }

  void Compiler::_break(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab << "break;" << std::endl;
  }

  void Compiler::_transaction(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    if (ast.args[0].token.value == "COMMIT") {
      code << tab << "Globals::memory()->commit_transaction();" << std::endl;
    } else if (ast.args[0].token.value == "ROLLBACK") {
      code << tab << "Globals::memory()->rollback_transaction();" << std::endl;
    } else if (ast.args[0].token.value == "BODY") {
      std::string obj = "o_" + std::to_string(std::rand());
      code << tab << "std::thread " << obj << "([&] () {" << std::endl;
      code << tab << TAB1 << "Globals::memory()->begin_transaction();" << std::endl;
      for (const Expression& expr : ast.args[0].args) {
	this->_clause(expr, code, lvl + 1);
      }
      code << tab << "});" << std::endl;
      code << tab << obj << ".join();" << std::endl;
    } else if (ast.args[0].token.value == "ISOLATION") {
      if (ast.args[0].args[0].token.value == "UNCOMMITTED") {
	code << tab << "Globals::memory()->transaction->set_isolation_level(Zigurat::IsolationLevel::READ_UNCOMMITTED);" << std::endl;
      } else if (ast.args[0].args[0].token.value == "COMMITTED") {
	code << tab << "Globals::memory()->transaction->set_isolation_level(Zigurat::IsolationLevel::READ_COMMITTED);" << std::endl;
      } else if (ast.args[0].args[0].token.value == "REPEATABLE") {
	code << tab << "Globals::memory()->transaction->set_isolation_level(Zigurat::IsolationLevel::REPEATABLE_READ);" << std::endl;
      } else if (ast.args[0].args[0].token.value == "SNAPSHOT") {
	code << tab << "Globals::memory()->transaction->set_isolation_level(Zigurat::IsolationLevel::SNAPSHOT);" << std::endl;
      } else if (ast.args[0].args[0].token.value == "SERIALIZABLE") {
	code << tab << "Globals::memory()->transaction->set_isolation_level(Zigurat::IsolationLevel::SERIALIZABLE);" << std::endl;
      }
    }
  }

  void Compiler::_try(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab << "try {" << std::endl;
    for (const Expression& expr : ast.args[0].args) {
      this->_clause(expr, code, lvl + 1);
    }
    code << tab << "} catch (" << this->_type_name(ast.args[2]) << "& " << ast.args[1].token.value << ") {" << std::endl;
    for (const Expression& expr : ast.args[3].args) {
      this->_clause(expr, code, lvl + 1);
    }
    code << tab << '}' << std::endl;
  }

  void Compiler::_throw(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab << "throw ";
    this->_expr.compile(ast.args[0], code);
    code << ';' << std::endl;
  }

  Compiler::~Compiler()
  {

  }
	
}
