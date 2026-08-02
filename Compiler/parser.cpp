#include "parser.hpp"
#include "tokenizer.hpp"
#include "parseexception.hpp"
#include "utility.hpp"
#include <regex>
#include "expression.hpp"


namespace Zigurat
{

  Parser::Parser()
  {

  }

  Parser::Parser(std::string patterns_file, bool trace)
    : _config(patterns_file), _trace(trace)
  {
    if (this->_trace)
      this->_config.print();
  }

  void Parser::configure(std::string patterns_file, bool trace)
  {
    _config.load(patterns_file);
    _trace = trace;
  }

  std::string Parser::_token_type(TokenType type)
  {
    switch (type) {
    case TokenType::BOOL:
      return "BOOL";
    case TokenType::INT:
      return "INT";
    case TokenType::FLOAT:
      return "FLOAT";
    case TokenType::STR:
      return "STR";
    case TokenType::NAME:
      return "NAME";
    case TokenType::OP:
      return "OP";
    case TokenType::LPAR:
      return "LPAR";
    case TokenType::RPAR:
      return "RPAR";
    case TokenType::EOF_:
      return "EOF";
    }
    return "UNKNOWN";
  }

  bool Parser::_parse_options(const Option& option)
  {
    if (option.options.size() == 0 || this->_eof)
      return true;

    this->_trace_level++;
    auto tmp_iter = this->_iter;
    auto tmp_trace_stack = this->_trace_stack;
    Expression* tmp_proc = this->_proc;
      
    bool result = false;
    for (const Option& ch_option : option.options) {

      if (ch_option.attr == "E") {
	tmp_proc->args.emplace_back(*tmp_iter);
	result = this->_parse(ch_option);  
	if (!result) {
	  tmp_proc->args.pop_back();
	}
      } else if (ch_option.attr == "P") {
	Expression ch_expr(*tmp_iter);
	this->_proc = &ch_expr;
	result = this->_parse(ch_option);  
	if (result) {
	  tmp_proc->args.push_back(ch_expr);
	}
      } else if (ch_option.attr == "O") {
	Expression ch_expr(*tmp_iter);
	this->_proc = &ch_expr;
	result = this->_parse(ch_option);  
	if (result) {
	  if (tmp_proc->args.size() > 0) {
	    ch_expr.args.insert(ch_expr.args.begin(), tmp_proc->args.back());
	    tmp_proc->args.pop_back();
	  }
	  tmp_proc->args.push_back(ch_expr);
	}
      } else if (ch_option.attr[0] == '$') {
	std::string attr = ch_option.attr;
	attr.erase(attr.begin());
	Expression ch_expr(Token(TokenType::NAME, attr, tmp_iter->line_no, tmp_iter->column_no));
	this->_proc = &ch_expr;
	result = this->_parse(ch_option);  
	if (result && (ch_expr.args.size() > 0 || ch_option.key != "OPTION")) {
	  tmp_proc->args.push_back(ch_expr);
	}
      } else {
	result = this->_parse(ch_option);  
      }

      this->_proc = tmp_proc;

      if (this->_return) {
	break;
      } else if (result && ch_option.key == "LOOP") {
	tmp_iter = this->_iter;
	continue;
      } else if (result && ch_option.key == "SERIE") {
	tmp_iter = this->_iter;
	continue;
      } else if (!result && ch_option.key == "SERIE") {
	tmp_iter = this->_iter;
	break;
      } else if (result) {
	break;
      } else {
	this->_iter = tmp_iter;
      }
    }

    if (result)
      this->_trace_stack = tmp_trace_stack;

    this->_proc = tmp_proc;
    this->_trace_level--;
    return result;
  }
  
  bool Parser::_parse(const Option& option)
  {
    if (this->_trace) {
      std::cout << std::string(this->_trace_level * 2, ' ') << "{" << option.attr << "} " << option.key << " '" << option.value 
		<< "' -> " << this->_token_type(this->_iter->type) << " '" << this->_iter->value << "'" << std::endl;
    }

    std::string key_pure;
    
    bool key_re = false;
    if (option.key.front() == 'r') {
      key_pure = option.key;
      key_pure.erase(key_pure.begin());
      key_re = true;
    }

    bool key_nofwd = false;
    if (option.key.back() == '?') {
      if (key_pure.size() > 0) {
	key_pure.pop_back();
      } else {
	key_pure = option.key;
	key_pure.pop_back();
      }
      key_nofwd = true;
    }

    if (this->_eof) {
      return true;
    } else if (option.key == "RET") {
      this->_return = true;
      if (option.value == "0")
        this->_return_value = true;
      else
	this->_return_value = false;
      return true;
    } else if (option.key == "EOF" && this->_iter->type == TokenType::EOF_) {
      
      this->_eof = true;
      return true;
        
    } else if (key_re && !key_nofwd) {
      
      try {
	std::regex re_key(key_pure, std::regex_constants::extended);
	if (std::regex_match(this->_token_type(this->_iter->type), re_key)) {
	  if (option.value.size() == 0) {
	    this->_iter++;
	    return this->_parse_options(option);
	  } else {
	    std::regex re(option.value, std::regex_constants::extended);
	    if (std::regex_match(this->_iter->value, re)) {
	      this->_iter++;
	      return this->_parse_options(option);
	    }
	  }
	}
      } catch (const std::regex_error& e) {
	throw ParseException(e.what(), *(this->_iter));	  
      }
      
    } else if (!key_re && key_nofwd) {
      
      if (key_pure == this->_token_type(this->_iter->type)) {
	if (option.value.size() == 0) {
	  return this->_parse_options(option);
	} else {
	  try {
	    std::regex re(option.value, std::regex_constants::extended);
	    if (std::regex_match(this->_iter->value, re)) {
	      return this->_parse_options(option);
	    }
	  } catch (const std::regex_error& e) {
	    throw ParseException(e.what(), *(this->_iter));	  
	  }
	}
      }

    } else if (key_re && key_nofwd) {

      try {
	std::regex re_key(key_pure, std::regex_constants::extended);
	if (std::regex_match(this->_token_type(this->_iter->type), re_key)) {
	  if (option.value.size() == 0) {
	    return this->_parse_options(option);
	  } else {
	    std::regex re(option.value, std::regex_constants::extended);
	    if (std::regex_match(this->_iter->value, re)) {
	      return this->_parse_options(option);
	    }
	  }
	}
      } catch (const std::regex_error& e) {
	throw ParseException(e.what(), *(this->_iter));	  
      }
      
    } else if (option.key == this->_token_type(this->_iter->type)) {

      if (option.value.size() == 0) {
        this->_iter++;
	return this->_parse_options(option);
      } else {
	try {
	  std::regex re(option.value, std::regex_constants::extended);
	  if (std::regex_match(this->_iter->value, re)) {
	    this->_iter++;
	    return this->_parse_options(option);
	  }
	} catch (const std::regex_error& e) {
	  throw ParseException(e.what(), *(this->_iter));	  
	}
      }
      
    } else if (option.key == "LOOP") {
      
      bool tmp_break_loop = this->_break_loop;
      int tmp_trace_level = this->_trace_level;
      bool result = false;

      do {
	this->_trace_level = tmp_trace_level;

	result = this->_parse_options(option);

	if (this->_trace)
	  std::cout << std::string(this->_trace_level * 2, ' ') << "LOOP -> " 
		    << ((result && !this->_break_loop && !this->_eof) ? "CONTINUE" : "BREAK") << std::endl;

      } while (result && !this->_break_loop && !this->_eof);

      this->_break_loop = tmp_break_loop;
      return result;

    } else if (option.key == "PROC") {
      
      if (option.value.size() > 0) {

	auto tmp_iter = this->_iter;
	int tmp_trace_level = this->_trace_level;

	this->_level++;
	bool result = this->_parse(option.value);

	if (this->_return) {
	  result = this->_return_value;
	  this->_return = false;
	}
	
	if (this->_trace)
	  std::cout << std::string(tmp_trace_level * 2, ' ') << option.value << " -> " << ((result) ? "TRUE" : "FALSE") << std::endl;
	
	this->_trace_level = tmp_trace_level;

	if (result) {
	  result = this->_parse_options(option);
	} else {
	  this->_iter = tmp_iter;
	}
	
	this->_level--;
	return result;

      } else {
	throw ParseException("PROC pattern must get 1 argument: (string)", *(this->_iter));
      }
      
    } else if (option.key == "MACRO") {
      
      if (option.value.size() > 0) {
        bool result = this->_parse(option.value);
	if (result) {
	  result = this->_parse_options(option);
	}
	return result;
      } else {
	throw ParseException("MACRO pattern must get 1 argument: (string)", *(this->_iter));
      }

    } else if (option.key == "SERIE") {
      
      return this->_parse_options(option);

    } else if (option.key == "LEVEL") {
      
      if (option.value.size() > 0) {

	if (option.value.back() == '!') {
	  std::string level_str = option.value;
	  level_str.pop_back();
	  int level = std::stoi(level_str);
	  if (level != this->_level)
	    return this->_parse_options(option);
	} else {
	  int level = std::stoi(option.value);
	  if (level == this->_level)
	    return this->_parse_options(option);
	}
	
      } else {
	throw ParseException("LEVEL pattern must get 1 argument: (int)", *(this->_iter));
      }
      return this->_parse(option.value);

    } else if (option.key == "OPTION") {
      
      auto tmp_iter = this->_iter;
      auto tmp_trace_stack = this->_trace_stack;
      bool result = this->_parse_options(option);

      if (!result) {
	this->_iter = tmp_iter;
      }

      this->_trace_stack = tmp_trace_stack;
      return true;

    } else if (option.key == "FORWARD") {
      
      this->_references[option.value]++;
      return this->_parse_options(option);

    } else if (option.key == "BACK") {
      
      this->_references[option.value]--;
      return this->_parse_options(option);

    } else if (option.key == "CHECK") {
      
      if (this->_references.find(option.value) != this->_references.end()) {
	return this->_parse_options(option);
      } else {
	return false;
      }

    } else if (option.key == "BREAK") {
    
      this->_break_loop = true;
      return this->_parse_options(option);

    } else {

    }

    this->_trace_stack.emplace_back(this->_trace_level, *(this->_iter));
    return false;
  }

  bool Parser::_parse(std::string pattern)
  {
    for (const Option& option : this->_config.root().options) {
      if (option.key == pattern) {
	return this->_parse_options(option);
      }
    }

    return false;
  }

  Expression Parser::parse(std::string pattern, std::list<Token>& tokens)
  {
    if (this->_trace)
      Zigurat::Tokenizer::print_tokens(tokens);
    
    this->_level = 0;
    this->_break_loop = false;
    // _eof and _return survive from the previous parse otherwise. Once _eof is
    // set every option succeeds without consuming a token, so a reused Parser
    // silently returned an empty tree for its second and every later source --
    // and the server keeps exactly one Parser for the life of the process.
    this->_eof = false;
    this->_return = false;
    this->_return_value = false;
    this->_tokens = &tokens;
    this->_iter = tokens.begin();
    this->_references.clear();
    this->_trace_stack.clear();

    Expression expr(Token(TokenType::NAME, pattern, 0, 0));
    this->_proc = &expr;
    bool result = this->_parse(pattern);

    if (result) {
      for (auto iter = this->_references.begin(); iter != this->_references.end(); iter++) {
	if (iter->second > 0)
	  throw ParseException("missing back reference of '" + iter->first + "'");
	else if (iter->second < 0)
	  throw ParseException("missing forward reference to '" + iter->first + "'");
      }
    } else {
      if (this->_iter->type == TokenType::EOF_){
	throw ParseException("syntax error at eof");
      } else {
	std::pair<int, Token> depth_error = this->_trace_stack.front();
	for (std::pair<int, Token> error : this->_trace_stack) {
	  if (error.first > depth_error.first)
	    depth_error = error;
	}
	throw ParseException("syntax error", depth_error.second);
      }
    }

    if (this->_trace)
      Parser::print_ast(expr);

    return expr;
  }

  void Parser::_print_ast(const Expression& ast, size_t lvl, size_t index, size_t pad_left)
  {
    if (index == 0) {
      if (lvl == 0)
	std::cout << "(";
      else
	std::cout << ' ' << "(";
    } else {
      std::cout << std::string(pad_left, ' ') << "(";
    }
    //ALGOLTokenizer::print_expr_proc(ast.procedure);
    //std::cout << " ";
    std::cout << ast.token.value;

    size_t i = 0;
    for (auto iter = ast.args.begin(); iter != ast.args.end(); iter++, i++) {

      Parser::_print_ast(*iter, lvl + 1, i, pad_left + ast.token.value.size() + 2);

      if (i != ast.args.size() - 1)
	std::cout << std::endl;
    }
  
    std::cout << ")";
  }
	
  void Parser::print_ast(const Expression& ast)
  {
    Parser::_print_ast(ast, 0, 0, 0);
    std::cout << std::endl;
  }

}
