#include "tokenizer.h"
#include "tokenizeexception.h"
#include <iostream>
#include <algorithm>
#include "token.h"
#include "utility.h"


namespace Zigurat
{

  const std::string Tokenizer::DIGITS = "0123456789";
  const std::string Tokenizer::ALPHABETS = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const std::string Tokenizer::OPERATORS = "+-*/%&|^<=>,.:;()";
  const std::string Tokenizer::SPACES = " \r\n\t\v";
  const std::vector<std::string> Tokenizer::MIXED_OPERATORS = {"::", "<=", "==", "<>", ">="};
  const std::vector<std::string> Tokenizer::NAME_OPERATORS = {"NOT", "AND", "OR", "IS", "LIKE", "BETWEEN"};
  
  void Tokenizer::tokenize(std::string code, std::list<Token>& tokens)
  {
    code.push_back('\n');
    char mode = '?';
    size_t lineno = 1;
    size_t colno = 0;
    size_t blineno = 1;
    size_t bcolno = 0;

    std::string::const_iterator bmode;
    std::string::const_iterator emode;

    for (auto iter = code.begin(); iter != code.end(); iter++) {
      const char wch = *iter;
      
      if (wch == '\n') {
	lineno++;
	colno = 0;
	if (mode == '#')
	  mode = '?';
      } else if (wch == '\r') {

      } else {
	colno++;
      }

      if (mode == '?' && wch == '-' && iter != code.end() && *(iter + 1) == '-') {
	mode = '#';
      } else if (mode == '?' && wch == '/' && iter != code.end() && *(iter + 1) == '*') {
	mode = '/';
      } else if (mode == '/' && wch == '*' && iter != code.end() && *(iter + 1) == '/') {
	iter++;
	mode = '?';
      } else if (mode == '#' || mode == '/') {
	continue;
      } else if (mode == '?' && wch == '\'' && (iter == code.begin() || *(iter - 1) != '\\')) {
	mode = 's';
	bmode = iter;
	blineno = lineno;
	bcolno = colno;
      } else if (mode == 's' && wch == '\'' && *(iter - 1) == '\\') {
	iter = code.erase(iter - 1);
      } else if (mode == 's' && wch == '\'') {
	emode = iter;
	bmode++;
	tokens.emplace_back(TokenType::STR, "R\"ZIP0ML0S0(" + std::string(bmode, emode) + ")ZIP0ML0S0\"", blineno, bcolno);
	mode = '?';
      } else if (mode == '?' && wch == '"' && (iter == code.begin() || *(iter - 1) != '\\')) {
	mode = 'c';
	bmode = iter;
	blineno = lineno;
	bcolno = colno;
      } else if (mode == 'c' && wch == '"' && *(iter - 1) == '\\') {
	iter = code.erase(iter - 1);
      } else if (mode == 'c' && wch == '"') {
	emode = iter;
	bmode++;
	std::string tmp(bmode, emode);
	size_t pos = -2;
	while ((pos = tmp.find('\'', pos + 2)) != std::string::npos) {
	  tmp.replace(pos, 1, "\\\'");
	}
	tokens.emplace_back(TokenType::STR, "'" + tmp + "'", blineno, bcolno);
	mode = '?';
      } else if (mode == 'c') {
	if (iter - bmode > 1)
	  throw TokenizeException("syntax error", lineno, colno, wch);
      } else if (mode == '?' && Tokenizer::DIGITS.find(wch) != std::string::npos) {
	mode = 'i';
	bmode = iter;
	blineno = lineno;
	bcolno = colno;
      } else if (mode == 'i' && wch == '.') {
	mode = 'f';
      } else if (mode == 'i' && Tokenizer::DIGITS.find(wch) == std::string::npos) {
	emode = iter;
	if (wch == 'l' || wch == 'L') {
	  if (*(emode - 1) == 'l' || *(emode - 1) == 'L')
	    throw TokenizeException("syntax error", lineno, colno, wch);
	} else if (wch == 'u' || wch == 'U') {
	  if (*(emode - 1) == 'l' || *(emode - 1) == 'L' || *(emode - 1) == 'u' || *(emode - 1) == 'U')
	    throw TokenizeException("syntax error", lineno, colno, wch);
	} else {
	  tokens.emplace_back(TokenType::INT, std::string(bmode, emode), blineno, bcolno);
	  iter--;
	  colno--;
	  mode = '?';
	}
      } else if (mode == 'f' && Tokenizer::DIGITS.find(wch) == std::string::npos) {
	emode = iter;
	if (wch == 'f') {
	  if (*(emode - 1) == 'f')
	    throw TokenizeException("syntax error", lineno, colno, wch);
	} else {
	  tokens.emplace_back(TokenType::FLOAT, std::string(bmode, emode), blineno, bcolno);
	  iter--;
	  colno--;
	  mode = '?';
	}
      } else if (mode == '?' && (wch == '@' || Tokenizer::ALPHABETS.find(wch) != std::string::npos)) {
	mode = 'n';
	bmode = iter;
	blineno = lineno;
	bcolno = colno;
      } else if (mode == '?' && (wch == '`' || Tokenizer::ALPHABETS.find(wch) != std::string::npos)) {
	mode = 'n';
	bmode = iter;
	blineno = lineno;
	bcolno = colno;
      } else if (mode == 'n' && Tokenizer::ALPHABETS.find(wch) == std::string::npos && Tokenizer::DIGITS.find(wch) == std::string::npos) {
	mode = '?';
	emode = iter;
	std::string name;
	bool to_upper = true;
	for (; bmode != emode; bmode++) {
	  if (*bmode == '`')
	    to_upper = false;
	  else if (to_upper)
	    name.push_back(std::toupper(*bmode));
	  else
	    name.push_back(*bmode);
	}
	if (name == "TRUE" || name == "FALSE") {
	  tokens.emplace_back(TokenType::BOOL, name, blineno, bcolno);
	} else if (std::find(Tokenizer::NAME_OPERATORS.begin(), Tokenizer::NAME_OPERATORS.end(), name) != Tokenizer::NAME_OPERATORS.end()) {
	  tokens.emplace_back(TokenType::OP, name, blineno, bcolno);
	} else {
	  tokens.emplace_back(TokenType::NAME, name, blineno, bcolno);
	}
	if (wch != '\r' && wch != '\n') {
	  iter--;
	  colno--;
	}
      } else if (mode == '?' && Tokenizer::OPERATORS.find(wch) != std::string::npos) {
	switch (wch) {
	case '(':
	  tokens.emplace_back(TokenType::LPAR, "(", lineno, colno);
	  break;
	case ')':
	  tokens.emplace_back(TokenType::RPAR, ")", lineno, colno);
	  break;
	default:
	  mode = 'o';
	  bmode = iter;
	  blineno = lineno;
	  bcolno = colno;
	  break;
	}
      } else if (mode == 'o' && (wch == '*' || wch == ',' || wch == ';' || wch == '(' || wch == ')' || 
				 Tokenizer::OPERATORS.find(wch) == std::string::npos)) {
	mode = '?';
	emode = iter;
	std::string opr;
	for (; bmode != emode; bmode++) {
	  opr.push_back(std::toupper(*bmode));
	}
	if (opr.size() == 1) {
	  tokens.emplace_back(TokenType::OP, opr, blineno, bcolno);
	} else if (std::find(Tokenizer::MIXED_OPERATORS.begin(), Tokenizer::MIXED_OPERATORS.end(), opr) != Tokenizer::MIXED_OPERATORS.end()) {
	  tokens.emplace_back(TokenType::OP, opr, blineno, bcolno);
	} else {
	  throw TokenizeException("syntax error", lineno, colno, wch);
	}
	if (wch != '\r' && wch != '\n') {
	  iter--;
	  colno--;
	}
      } else if (mode == '?' && Tokenizer::SPACES.find(wch) == std::string::npos) {
	throw TokenizeException("syntax error", lineno, colno, wch);
      } else {
	
      }
    }
    
    if (mode != '?') {
      throw TokenizeException("syntax error", blineno, bcolno, *bmode);
    }

    tokens.emplace_back(TokenType::EOF_, "", 0, 0);
  }

  void Tokenizer::print_tokentype(TokenType type)
  {
    switch (type) {
    case TokenType::BOOL:
      std::cout << "BOOL";
      break;
    case TokenType::INT:
      std::cout << "INT";
      break;
    case TokenType::FLOAT:
      std::cout << "FLOAT";
      break;
    case TokenType::STR:
      std::cout << "STR";
      break;
    case TokenType::NAME:
      std::cout << "NAME";
      break;
    case TokenType::OP:
      std::cout << "OP";
      break;
    case TokenType::LPAR:
      std::cout << "LPAR";
      break;
    case TokenType::RPAR:
      std::cout << "RPAR";
      break;
    case TokenType::EOF_:
      std::cout << "EOF";
      break;
    }
  }  

  void Tokenizer::print_tokens(std::list<Token>& tokens)
  {
    for (Token& token : tokens) {
      std::cout << "(";
      Tokenizer::print_tokentype(token.type);
      std::cout << " " << token.value << " line " << token.line_no << " column " << token.column_no << ")" << std::endl;
    }
  }

}

