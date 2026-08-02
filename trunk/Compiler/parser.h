
#ifndef __PARSER_H__
#define __PARSER_H__

#include <string>
#include <list>
#include <map>
#include "configuration.h"

namespace Zigurat
{

  class Option;
  class Expression;
  class Token;
  enum class TokenType;

  class Parser
  {
  public:
    Parser();
    Parser(std::string, bool);  
    void configure(std::string, bool);
    Expression parse(std::string, std::list<Token>&);  
    static void print_ast(const Expression&);
  		
  private:
    std::string _token_type(TokenType);
    bool _parse_options(const Option&);
    bool _parse(const Option&);
    bool _parse(std::string);
    
    static void _print_ast(const Expression&, size_t, size_t, size_t);
    
    Configuration _config;
    bool _trace;
    int _trace_level = 0;
    bool _eof = false;
    bool _return = false;
    bool _return_value;
    int _level = 0;
    bool _break_loop = false;
    std::list<Token>* _tokens = nullptr; 
    std::list<Token>::iterator _iter;
    std::map<std::string, int> _references;
    std::list< std::pair<int, Token> > _trace_stack;
    Expression* _proc = nullptr;
  };

}

#endif // __PARSER
