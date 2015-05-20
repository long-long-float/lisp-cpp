#include <iostream>
#include <string>
#include <vector>
#include <ctype.h>

enum TokenType{
  TOKEN_BRACKET_OPEN,
  TOKEN_BRACKET_CLOSE,
  TOKEN_SYMBOL,
  TOKEN_STRING,
};

class Token {
public:
  TokenType type;
  std::string value;

  Token(TokenType atype) : type(atype), value(std::string()) {
  }

  Token(TokenType atype, std::string avalue) : type(atype), value(avalue) {
  }
};

bool is_symbol(char c) {
  return isalnum(c) && isalpha(c);
}

int main() {
  using namespace std;

  string code;
  //code.reserve(1024);
  vector<Token*> tokens;

  char ch;
  while((ch = cin.get()) != std::char_traits<char>::eof()) {
    code.push_back(ch);
  }

  for(size_t i = 0 ; i < code.size() - 1 ; i++) { //TODO: -1を修正(EOFっぽい?)
    char ch = code[i];
    if(ch == '(')
      tokens.push_back(new Token(TOKEN_BRACKET_OPEN));
    else if(ch == ')')
      tokens.push_back(new Token(TOKEN_BRACKET_CLOSE));
    else if(ch == '"') { // string
      i++;

      size_t token_len = 0;
      while(code[i + token_len] != '"') {
        token_len++;
        if(i + token_len >= code.size()) {
          //TODO: raise an error
        }
      }
      tokens.push_back(new Token(TOKEN_STRING, code.substr(i, token_len)));
      i += token_len;
    }
    else if(ch == ' ')
      ;//skip
    else { // symbol
      size_t token_len = 0;
      while(is_symbol(code[i + token_len])) {
        token_len++;
        if(i + token_len >= code.size()) {
          //TODO: raise an error
        }
      }
      tokens.push_back(new Token(TOKEN_SYMBOL, code.substr(i, token_len)));
      i += token_len - 1;
    }
  }

  for(Token *token : tokens) {
    cout << "token type: " << token->type << ", value: " << token->value << endl;
  }

  // destruction
  for(size_t i = 0 ; i < tokens.size() ; i++) {
    delete tokens[i];
  }

  return 0;
}
