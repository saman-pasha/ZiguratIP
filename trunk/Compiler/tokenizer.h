
#ifndef __TOKENIZER_H__
#define __TOKENIZER_H__

#include "tokentype.h"
#include <string>
#include <list>
#include <vector>

namespace Zigurat
{

  class Token;

  class Tokenizer
  {
  public:
    static const std::string DIGITS;
    static const std::string ALPHABETS;
    static const std::string OPERATORS;
    static const std::string SPACES;
    static const std::vector<std::string> MIXED_OPERATORS;
    static const std::vector<std::string> NAME_OPERATORS;
  
    static void tokenize(std::string, std::list<Token>&);
    
    static void print_tokentype(TokenType);
    static void print_tokens(std::list<Token>&);
  };

}

#endif // __TOKENIZER_H__

