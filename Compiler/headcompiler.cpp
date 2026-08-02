#include "headcompiler.hpp"
#include "compiler.hpp"
#include "compileexception.hpp"


namespace Zigurat
{

  HeadCompiler::HeadCompiler(const Compiler& compiler, std::initializer_list<const Expression> locals)
    : _compiler(compiler)
  {
    for (const Expression& from : locals) {
      std::string name((from.args.size() > 1) ? compiler._name(from.args[1]) : compiler._name(from.args[0]));
      std::string type_name(compiler._type_name(from.args[0]));
      this->_locals.emplace_back(type_name, name);
      this->_catalog.load(compiler._catalog_path + compiler._include_name(from.args[0]) + ".conf");
    }
  }

  void HeadCompiler::compile(const Expression& ast, std::stringstream& code) const
  {
    int counter = 1;
    if (ast.args.size() == 2) {
      if (ast.args[1].token.type == TokenType::STR) {
	if (ast.args[1].token.value.find(',') != std::string::npos)
	  throw CompileException("invalid character ','", ast.args[1]);	
	code << ast.args[1].token.value.substr(12, ast.args[1].token.value.size() - 23);
      } else {
	code << ast.args[1].token.value;
      }
    } else if (ast.args[0].token.value == "$obj") {
      this->_alias(ast.args[0].args[0], code, counter);
    } else {
      code << "expr_" << std::to_string(counter++);
    }
  }

  void HeadCompiler::_alias(const Expression& expr, std::stringstream& code, int& counter) const
  {
    if (expr.args.size() == 0 && expr.token.value == "*") { // Select All Columns *
      std::list<const Option*> opts;
      if (this->_catalog.extract("/$TABLE/COLUMNS/COLUMN/$NAME", opts)) {
	const Option* opt = *opts.begin();
	std::string type_name;
	this->_catalog.get(*opt, "/TYPE_NAME", type_name);
	for (const std::pair<std::string, std::string>& pair : this->_locals) {
	  if (type_name == pair.first) {
	    for (auto iter = ++(opts.begin()); iter != opts.end(); iter++)
	      code << (*iter)->value << ',';
	    code.seekp(-1, std::ios::cur);
	    return;
	  }
	}
      }  
      code << expr.token.value;

    } else if (expr.token.value == "." && expr.args[1].token.value == "*") { // Select All Columns table.*
      for (const std::pair<std::string, std::string>& pair : this->_locals) {
	std::cout << pair.first << "," << pair.second << std::endl;
	if (expr.args[0].token.value == pair.second) {
	  std::list<const Option*> opts;
	  if (this->_catalog.extract("/$TABLE/TYPE_NAME:" + pair.first, opts)) {
	    const Option* opt = *opts.begin();
	    if (this->_catalog.list(*opt, "/COLUMNS/COLUMN/NAME", opts)) {
	      for (auto iter = ++opts.begin(); iter != opts.end(); iter++)
		code << (*iter)->value << ',';
	      code.seekp(-1, std::ios::cur);
	      return;
	    }
	  }
	}
      }
      code << expr.token.value;

    } else if (expr.args.size() == 0) {
      code << expr.token.value;
    } else if (expr.args.size() == 2 && expr.args[0].token.value == ".") {
      this->_alias(expr.args[1], code, counter);
    } else if (expr.args.size() == 3 && expr.args[0].token.value == ":") {
      this->_alias(expr.args[2], code, counter);
    } else {
      code << "expr_" + std::to_string(counter++);
    }      
  };

}
