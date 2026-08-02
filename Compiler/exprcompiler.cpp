#include "exprcompiler.h"
#include "compiler.h"
#include "compileexception.h"


namespace Zigurat
{

  ExprCompiler::ExprCompiler(const Compiler& compiler, std::initializer_list<const Expression> locals)
    : _compiler(compiler)
  {
    for (const Expression& from : locals) {
      std::string name((from.args.size() > 1) ? compiler._name(from.args[1]) : compiler._name(from.args[0]));
      std::string type_name(compiler._type_name(from.args[0]));
      this->_locals.emplace_back(type_name, name);
      this->_catalog.load(compiler._catalog_path + compiler._include_name(from.args[0]) + ".conf");
    }
  }

  void ExprCompiler::compile(const Expression& ast, std::stringstream& code) const
  {
    if (ast.token.value == "$obj") {
      this->_object(ast.args[0], code);
    } else if (ast.token.value == "$tmpl") {
      code << "< ";
      bool has_args = false;
      for (const Expression& expr : ast.args) {
	code << this->_compiler._type_name(expr) << ", ";
	has_args = true;
      }
      if (has_args)
	code.seekp(-2, std::ios::cur);
      code << " >";
    } else if (ast.token.value == "$args") {
      code << '(';
      bool has_args = false;
      for (const Expression& expr : ast.args) {
	this->compile(expr, code);
	code << ", ";
	has_args = true;
      }
      if (has_args)
	code.seekp(-2, std::ios::cur);
      code << ')';
    } else if (ast.token.type == TokenType::OP) {
      if (!(ast.token.value == "." || ast.token.value == "::"))
	code << '(';
      if (ast.args.size() == 1) {
	if (ast.token.value == "NOT") {
	  code << "!";
	} else {
	  code << ast.token.value;
	}
	this->compile(ast.args[0], code);
      } else if (ast.args.size() == 2) {
	this->compile(ast.args[0], code);
	if (ast.token.value == "=") {
	  code << "==";
	} else if (ast.token.value == "<>") {
	  code << "!=";
	} else if (ast.token.value == "AND") {
	  code << "&&";
	} else if (ast.token.value == "OR") {
	  code << "||";
	} else if (ast.token.value == "IS") {
	  code << "==";
	} else if (ast.token.value == "LIKE") {
	  code << "==";
	} else {
	  code << ast.token.value;
	}
	this->compile(ast.args[1], code);
      } else {
	throw CompileException("wrong expression", ast);
      }
      if (!(ast.token.value == "." || ast.token.value == "::"))
	code << ')';
    } else if (ast.token.type == TokenType::NAME) {
      code << ast.token.value;
      if (ast.args.size() > 0) {
	this->compile(ast.args[0], code);
      }
      if (ast.args.size() > 1) {
	this->compile(ast.args[1], code);
      }
    } else {
      code << ast.token.value;
    }
  }


  void ExprCompiler::_object(const Expression& ast, std::stringstream& code) const
  {
    if (ast.token.type == TokenType::OP && ast.token.value == "*") { // Select All Columns *

      std::list<const Option*> opts;
      if (this->_catalog.extract("/$TABLE/COLUMNS/COLUMN/$NAME", opts)) {
	const Option* opt = *opts.begin();
	std::string type_name;
	this->_catalog.get(*opt, "/TYPE_NAME", type_name);
	for (const std::pair<std::string, std::string>& pair : this->_locals) {
	  if (type_name == pair.first) {
	    for (auto iter = ++(opts.begin()); iter != opts.end(); iter++)
	      code << pair.second << ".*" << pair.second << '.' << (*iter)->value << ',';
	    code.seekp(-1, std::ios::cur);
	    return;
	  }
	}
      }  
      this->compile(ast, code);

    } else if (ast.token.type == TokenType::OP && ast.token.value == "." && ast.args[1].token.value == "*" ) { // Select All Columns table.*

      for (const std::pair<std::string, std::string>& pair : this->_locals) {
	std::cout << pair.first << "," << pair.second << std::endl;
	if (ast.args[0].token.value == pair.second) {
	  std::list<const Option*> opts;
	  if (this->_catalog.extract("/$TABLE/TYPE_NAME:" + pair.first, opts)) {
	    const Option* opt = *opts.begin();
	    if (this->_catalog.list(*opt, "/COLUMNS/COLUMN/NAME", opts)) {
	      for (auto iter = ++opts.begin(); iter != opts.end(); iter++)
		code << pair.second << ".*" << pair.second << '.' << (*iter)->value << ',';
	      code.seekp(-1, std::ios::cur);
	      return;
	    }
	  }
	}
      }
      this->compile(ast, code);

    } else if (ast.token.type == TokenType::OP && ast.args[0].token.value[0] != '@') {
      
      for (const std::pair<std::string, std::string>& pair : this->_locals) {
	if (ast.args[0].token.value == pair.second) {
	  code << pair.second << ".*" << pair.second << '.';
	  this->compile(ast.args[1], code);
	  return;
	}
      }

      this->compile(ast, code);
      
    } else if (ast.token.type == TokenType::OP && ast.args[0].token.value[0] == '@') {

      std::string val = ast.args[0].token.value;
      val.erase(val.begin());
      code << val << ast.token.value;
      this->compile(ast.args[1], code);

    } else if (ast.token.type == TokenType::NAME && ast.token.value[0] != '@') {
      
      std::list<const Option*> opts;
      if (this->_catalog.extract("/$TABLE/COLUMNS/COLUMN/NAME:" + ast.token.value, opts)) {
	const Option* opt = *opts.begin();
	std::string type_name;
	this->_catalog.get(*opt, "/TYPE_NAME", type_name);
	for (const std::pair<std::string, std::string>& pair : this->_locals) {
	  if (type_name == pair.first) {
	    code << pair.second << ".*" << pair.second << '.';
	    this->compile(ast, code);
	    return;
	  }
	}
      }
  
      this->compile(ast, code);

    } else if (ast.token.type == TokenType::NAME && ast.token.value[0] == '@') {

      std::string val = ast.token.value;
      val.erase(val.begin());
      code << val;

    } else {
      
      this->compile(ast, code);

    }
  }

}
