#include "compiler.hpp"
#include "compileexception.hpp"
#include "configuration.hpp"
#include "wherecompiler.hpp"
#include "headcompiler.hpp"
#include <sstream>


namespace Zigurat
{

  void Compiler::_insert(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    std::string obj = "o_" + std::to_string(std::rand());
    code << tab << this->_type_name(ast.args[0].args[0]) << ' ' << obj;
    if (ast.args[0].args.size() > 1) {
      code << ';' << std::endl;
      if (ast.args[0].args.size() - 1 != ast.args[1].args.size()) {
	throw CompileException("count of fields and values must be equal", ast);
      }
      for (size_t i = 1; i < ast.args[0].args.size(); i++) {
	code << tab << obj << ".*" << obj << '.' << ast.args[0].args[i].token.value << " = ";
	this->_expr.compile(ast.args[1].args[i - 1], code);
	code << ';' << std::endl;
      }
    } else {
      code << '(';
      for (const Expression& expr : ast.args[1].args) {
	this->_expr.compile(expr, code);
	code << ", ";
      }
      if (!ast.args[1].args.empty())
	code.seekp(-2, std::ios::cur);
      code << ");" << std::endl;
    }
    code << tab << "Globals::memory()->online_insert(" << obj << ");" << std::endl;
  }

  void Compiler::_select_content(const Expression& ast, std::stringstream& code, int lvl, bool echo_mode)
  {
    std::string tab(lvl, '\t');
    ExprCompiler* expr_compiler;
    
    if (ast.args[ast.args.size() - 1].token.value == "FROM") {
      expr_compiler = new ExprCompiler(*this, {ast.args[ast.args.size() - 1]});
    } else if (ast.args[ast.args.size() - 1].token.value == "WHERE") {
      expr_compiler = new ExprCompiler(*this, {ast.args[ast.args.size() - 2]});
    } 
    
    if (echo_mode) {
      for (const Expression& expr : ast.args) {
	if (expr.token.value == "FROM")
	  break;
	code << tab << "*Globals::echo_stream() << ";
	expr_compiler->compile(expr.args[0], code);
	code << ';' << std::endl;
      }
    } else {
      code << tab << "Globals::client_stream()->write_std_ubyte((uint8_t)Zigurat::ResultType::CURSOR_FETCH);" << std::endl;
      code << tab << "Globals::client_stream()->pack(" << std::endl;
      for (const Expression& expr : ast.args) {
	if (expr.token.value == "FROM")
	  break;
	code << tab << TAB1;
	expr_compiler->compile(expr.args[0], code);
	code << ", " << std::endl;
      }
      code .seekp(-2 - ENDL_LENGTH, std::ios::cur);
      code << ");" << std::endl;
    }

    delete expr_compiler;
  }

  void Compiler::_select(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    code << tab << "if (Globals::echo_stream() != nullptr) {" << std::endl;
    this->_select(ast, code, lvl + 1, true);
    code << tab << "} else {" << std::endl;
    this->_select(ast, code, lvl + 1, false);
    code << tab << "}" << std::endl;
  }

  void Compiler::_select(const Expression& ast, std::stringstream& code, int lvl, bool echo_mode)
  {
    std::string tab(lvl, '\t');

    size_t args_size = ast.args.size();
    const Expression* from = (ast.args[args_size - 1].token.value == "WHERE") ? &(ast.args[args_size - 2]) : &(ast.args[args_size - 1]);
    const Expression* where = (ast.args[args_size - 1].token.value == "WHERE") ? &(ast.args[args_size - 1]) : nullptr;;

    if (!echo_mode) {
      code << tab << "Globals::client_stream()->write_std_ubyte((uint8_t)Zigurat::ResultType::CURSOR_OPEN);" << std::endl;
      code << tab << "Globals::client_stream()->write_std_string(\"";

      HeadCompiler head_compiler(*this, {*from});

      for (const Expression& expr : ast.args) {
	if (expr.token.value == "FROM")
	  break;
      
	head_compiler.compile(expr, code);
	code << ",";
      }
      code.seekp(-1, std::ios::cur);
      code << "\");" << std::endl;
    }
    
    WhereCompiler where_compiler(*this, *from);
    where_compiler.compile(where, code, lvl, [&] (int lvl) {
	this->_select_content(ast, code, lvl, echo_mode);
      });

    if (!echo_mode) {
      code << tab << "Globals::client_stream()->write_std_ubyte((uint8_t)Zigurat::ResultType::CURSOR_CLOSE);" << std::endl;
    }
  }

  void Compiler::_update_content(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    std::string obj = "o_" + std::to_string(std::rand());
    std::string name = (ast.args[0].args.size() > 1) ? this->_name(ast.args[0].args[1]) : this->_name(ast.args[0].args[0]);
    std::string type_name = this->_type_name(ast.args[0].args[0]);
    
    ExprCompiler expr_compiler(*this, {ast.args[0]});
    
    code << tab << type_name << " " << obj << '(' << name << ");" << std::endl;
    for (const Expression& expr : ast.args[1].args) {
      code << tab << obj << ".*" << obj << "." << expr.token.value << " = ";
      expr_compiler.compile(expr.args[0], code);
      code << ';' << std::endl;
    }
    code << tab << "Globals::memory()->online_update(" << name << ", " << obj << ");" << std::endl;
  }

  void Compiler::_update(const Expression& ast, std::stringstream& code, int lvl)
  {
    const Expression& from = ast.args[0];
    WhereCompiler where_compiler(*this, from);
    
    if (ast.args.size() == 3) {
      const Expression& where = ast.args[2];
      
      where_compiler.compile(&where, code, lvl, [&] (int lvl) {
	  this->_update_content(ast, code, lvl);
	});
    } else {
      where_compiler.compile(nullptr, code, lvl, [&] (int lvl) {
	  this->_update_content(ast, code, lvl);
	});
    }
  }
	
  void Compiler::_delete_content(const Expression& ast, std::stringstream& code, int lvl)
  {
    std::string tab(lvl, '\t');
    std::string name = (ast.args[0].args.size() > 1) ? this->_name(ast.args[0].args[1]) : this->_name(ast.args[0].args[0]);

    code << tab << "Globals::memory()->online_delete(" << name << ");" << std::endl;
  }

  void Compiler::_delete(const Expression& ast, std::stringstream& code, int lvl)
  {
    const Expression& from = ast.args[0];
    WhereCompiler where_compiler(*this, from);
    
    if (ast.args.size() == 2) {
      const Expression& where = ast.args[1];
      
      where_compiler.compile(&where, code, lvl, [&] (int lvl) {
	  this->_delete_content(ast, code, lvl);
	});
    } else {
      where_compiler.compile(nullptr, code, lvl, [&] (int lvl) {
	  this->_delete_content(ast, code, lvl);
	});
    }
  }

}
