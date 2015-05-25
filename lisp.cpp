#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <list>
#include <typeinfo>
#include <stdexcept>
#include <ctype.h>

#define PRINT_LINE (std::cout << "line: " << __LINE__ << std::endl)

enum TokenType{
  TOKEN_BRACKET_OPEN,
  TOKEN_BRACKET_CLOSE,
  TOKEN_SYMBOL,
  TOKEN_STRING,
  TOKEN_NIL,
};

class Token {
public:
  TokenType type;
  std::string value;

  Token(TokenType atype) : type(atype), value(std::string()) {
  }

  Token(TokenType atype, std::string avalue) : type(atype), value(avalue) {
  }

  std::string str() {
    std::stringstream ss;
    ss << "Token(" << type << ", " << value << ")";
    return ss.str();
  }
};

namespace Lisp {
  class Expression;
}

// memory allocated exprs
std::list<Lisp::Expression*> objects;

namespace Lisp {
  class Expression {
  public:
    Expression() {
      objects.push_back(this);
    }
    virtual ~Expression() {}

    virtual std::string lisp_str() = 0;
  };

  class CallFunction : public Expression {
  public:
    std::string name;
    std::vector<Expression*> args;

    CallFunction(std::string aname, std::vector<Expression*> &aargs) : name(aname), args(aargs) {}

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

  class Symbol : public Expression {
  public:
    std::string value;

    Symbol(std::string &avalue) : value(avalue) {}

    std::string lisp_str() { return value; }
  };

  class List : public Expression {
  public:
    std::vector<Expression*> values;

    List(std::vector<Expression*> &avalues) : values(avalues) {}

    std::string lisp_str() {
      std::stringstream ss;
      ss << "(";
      for(Expression* value : values) {
        ss << value->lisp_str() << " ";
      }
      ss << ")";
      return ss.str();
    }
  };

  // TODO: RubyみたくExpression*に埋め込みたい
  class Nil : public Expression {
  public:
    Nil() {}

    std::string lisp_str() { return "nil"; }
  };

  class Parser {
  public:
    std::vector<Expression*> parse(const std::string &code) {
      tokens = tokenize(code);

      /*for(auto tok : tokens) {
        std::cout << tok->str() << std::endl;
      }*/

      std::vector<Expression*> exprs;
      while(!tokens.empty()) {
        exprs.push_back(parse_expr());
      }
      return exprs;
    }

  private:
    std::list<Token*> tokens;

    inline Token* cur_token() {
      return tokens.empty() ? nullptr : tokens.front();
    }

    void consume_token() {
      auto ctoken = cur_token();
      if(ctoken) delete ctoken;

      if(!tokens.empty()) tokens.pop_front();
    }

    Expression* parse_call_fun() {
      consume_token();
      if(cur_token()->type != TOKEN_SYMBOL) {
        //TODO: raise an error
      }
      std::string name;
      name = cur_token()->value;

      consume_token();
      std::vector<Expression*> args;
      for(; !tokens.empty() && cur_token()->type != TOKEN_BRACKET_CLOSE ; consume_token()) {
        args.push_back(parse_expr());
      }

      consume_token();

      return new CallFunction(name, args);
    }

    Expression* parse_expr() {
      switch(cur_token()->type) {
        case TOKEN_BRACKET_OPEN:
          return parse_call_fun();
        case TOKEN_STRING:
          return new String(cur_token()->value);
        case TOKEN_NIL:
          return new Nil();
        case TOKEN_SYMBOL:
          return new Symbol(cur_token()->value);
        default:
          //TODO: raise an error
          return nullptr;
      }
    }

    bool is_symbol(char c) {
      return isalnum(c) && isalpha(c);
    }

    std::list<Token*> tokenize(const std::string &code) {
      std::list<Token*> tokens;

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

          std::string token_val = code.substr(i, token_len);
          TokenType token_type;
          if(token_val == "nil")
            token_type = TOKEN_NIL;
          else
            token_type = TOKEN_SYMBOL;

          if(token_type == TOKEN_SYMBOL)
            tokens.push_back(new Token(TOKEN_SYMBOL, token_val));
          else
            tokens.push_back(new Token(token_type));

          i += token_len - 1;
        }
      }

      return tokens;
    }
  };

  class Evaluator {
  public:
    static Expression* evaluate(Expression* expr) {
      const std::type_info& id = typeid(*expr);
      if(id == typeid(CallFunction)) {
        auto call_fun = (CallFunction*)expr;
        auto name = call_fun->name;
        if(name == "print") {
          std::cout << (evaluate(call_fun->args[0]))->lisp_str() << std::endl;
          return new Nil();
        }
        else if(name == "list") {
          auto args = call_fun->args;
          for(size_t i = 0 ; i < args.size() ; i++) {
            args[i] = evaluate(args[i]);
          }
          return new List(args);
        }
      }

      return expr;
    }
  };
}


int main() {
  using namespace std;

  string code;
  //code.reserve(1024);

  char ch;
  while((ch = cin.get()) != std::char_traits<char>::eof()) {
    code.push_back(ch);
  }

  Lisp::Parser parser;
  auto exprs = parser.parse(code);

  for(size_t i = 0 ; i < exprs.size() ; i++) {
    exprs[i] = Lisp::Evaluator::evaluate(exprs[i]);
  }

  // fake GC
  for(auto expr : objects) {
    delete expr;
  }

  return 0;
}
