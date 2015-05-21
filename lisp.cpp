#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <typeinfo>
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

namespace Lisp {
  class Expression {
  public:
    virtual ~Expression() {}

    virtual std::string lisp_str() = 0;
  };

  class CallFunction : public Expression {
  public:
    std::string name;
    std::vector<Expression*> args;

    CallFunction(std::string aname, std::vector<Expression*> &aargs) : name(aname), args(aargs) {}
    ~CallFunction() {
      for(size_t i = 0 ; i < args.size() ; i++) {
        delete args[i];
      }
    }

    std::string lisp_str() {
      std::stringstream ss;
      ss << "(" << name << " ";
      for(Expression* arg : args) {
        ss << arg->lisp_str() << " ";
      }
      ss << ")";
      return ss.str();
    }
  };

  class String : public Expression {
  public:
    std::string value;

    String(std::string &avalue) : value(avalue) {}

    std::string lisp_str() { return '"' + value + '"'; }
  };
}

bool is_symbol(char c) {
  return isalnum(c) && isalpha(c);
}

int main() {
  using namespace std;

  string code;
  //code.reserve(1024);
  vector<Token*> tokens;
  vector<Lisp::Expression*> exprs;

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
    else if(ch == ' ' || ch == '\n')
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

  // parse
  for(size_t i = 0 ; i < tokens.size() ; i++) {
    Token* token = tokens[i];
    switch(token->type) {
      case TOKEN_BRACKET_OPEN: {
        string name;
        vector<Lisp::Expression*> args;

        i++;
        if(tokens[i]->type != TOKEN_STRING) {
          //TODO: raise an error
        }
        name = tokens[i]->value;

        i++;
        for(; tokens[i]->type != TOKEN_BRACKET_CLOSE ; i++) {
          args.push_back(new Lisp::String(tokens[i]->value));
        }

        exprs.push_back(new Lisp::CallFunction(name, args));

        break;
      }
      default:
        break;
    }
  }

  // evaluate
  for(auto expr : exprs) {
    const type_info& id = typeid(*expr);
    if(id == typeid(Lisp::CallFunction)) {
      auto call_fun = (Lisp::CallFunction*)expr;
      if(call_fun->name == "print") {
        for(auto arg : call_fun->args) {
          cout << ((Lisp::String*)arg)->value;
        }
      }
    }
  }

  // destruction
  for(size_t i = 0 ; i < tokens.size() ; i++) {
    delete tokens[i];
  }

  for(size_t i = 0 ; i < exprs.size() ; i++) {
    delete exprs[i];
  }

  return 0;
}
