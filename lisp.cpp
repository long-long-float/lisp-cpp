#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <list>
#include <map>
#include <stack>
#include <typeinfo>
#include <stdexcept>
#include <cstdlib>
#include <ctype.h>

#define PRINT_LINE (std::cout << "line: " << __LINE__ << std::endl)

enum TokenType{
  TOKEN_BRACKET_OPEN,
  TOKEN_BRACKET_CLOSE,
  TOKEN_SYMBOL,
  TOKEN_STRING,
  TOKEN_INTEGER,
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

  class String : public Expression {
  public:
    std::string value;

    String(std::string &avalue) : value(avalue) {}

    std::string lisp_str() { return '"' + value + '"'; }
  };

  class Integer : public Expression {
  public:
    unsigned long value;

    Integer(std::string &avalue)  : value(std::atol(avalue.c_str())) {}
    Integer(unsigned long avalue) : value(avalue) {}

    std::string lisp_str() { return std::to_string(value); }
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
      size_t i = 0;
      for(Expression* value : values) {
        ss << value->lisp_str();
        if(i < values.size() - 1) ss << " ";
        i++;
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

  class T : public Expression {
  public:
    T() {}

    std::string lisp_str() { return "T"; }
  };

  class Parser {
  public:
    std::vector<Expression*> parse(const std::string &code) {
      tokens = tokenize(code);

      if(false) { // NOTE: for debug
        for(auto tok : tokens) {
          std::cout << tok->str() << std::endl;
        }
      }

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

    Expression* parse_list() {
      consume_token();
      if(cur_token()->type != TOKEN_SYMBOL) {
        //TODO: raise an error
      }

      std::vector<Expression*> values;
      while(!tokens.empty() && cur_token()->type != TOKEN_BRACKET_CLOSE) {
        values.push_back(parse_expr());
      }

      return new List(values);
    }

    Expression* parse_expr() {
      Expression* ret;
      switch(cur_token()->type) {
        case TOKEN_BRACKET_OPEN:
          ret = parse_list();
          break;
        case TOKEN_STRING:
          ret = new String(cur_token()->value);
          break;
        case TOKEN_NIL:
          ret = new Nil();
          break;
        case TOKEN_SYMBOL:
          ret = new Symbol(cur_token()->value);
          break;
        case TOKEN_INTEGER:
          ret = new Integer(cur_token()->value);
          break;
        default:
          throw std::logic_error("unknown token: " + std::to_string(cur_token()->type));
      }
      consume_token();
      return ret;
    }

    bool is_symbol(char c) {
      return c == '!' || ('#' <= c && c <= '\'') || ('*' <= c && c <= '/') || isalpha(c);
    }

    bool is_number(char c) {
      return isdigit(c);
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
        else if(is_number(ch)) {
          size_t token_len = 0;
          while(is_number(code[i + token_len])) {
            token_len++;
            if(i + token_len >= code.size()) {
              // TODO: raise an error
            }
          }

          std::string token_val = code.substr(i, token_len);
          tokens.push_back(new Token(TOKEN_INTEGER, token_val));

          i += token_len - 1;
        }
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

  typedef std::map<std::string, Expression*> Environment;

  class Evaluator {
    std::stack<Environment> envs;

    Expression* eval_expr(Expression* expr) {
      const std::type_info& id = typeid(*expr);
      if(id == typeid(List)) {
        auto list = (List*)expr;
        auto name = ((Symbol*)list->values[0])->value;
        if(name == "print") {
          std::cout << (evaluate(list->values[1]))->lisp_str() << std::endl;
          return new Nil();
        }
        else if(name == "setq") {
          envs.top()[regard<Symbol>(list->values[1])->value] = list->values[2];
          return new Nil();
        }
        else if(name == "atom") {
          auto val = evaluate(list->values[1]);
          if(typeid(*val) != typeid(List)) return new T();
          else return new Nil();
        }
        else if(name == "+") {
          Integer* sum = new Integer(0);
          for(size_t i = 1 ; i < list->values.size() ; i++) {
            sum->value += regard<Integer>(evaluate(list->values[i]))->value;
          }
          return sum;
        }
        else if(name == "let") {
          Environment env;
          auto pairs = regard<List>(list->values[1])->values;
          for(auto pair : pairs) {
            auto kv = regard<List>(pair);
            env[regard<Symbol>(kv->values[0])->value] = kv->values[1];
          }
          envs.push(env);

          Expression* ret;
          for(size_t i = 2 ; i < list->values.size() ; i++) {
            ret = evaluate(list->values[i]);
          }

          envs.pop();

          return ret;
        }
        else if(name == "for") {
          auto counter_name = regard<Symbol>(list->values[1]);
          auto start        = regard<Integer>(evaluate(list->values[2]));
          auto end          = regard<Integer>(evaluate(list->values[3]));

          auto counter      = new Integer(start->value);

          Environment env;
          env[counter_name->value] = counter;
          envs.push(env);

          for(; counter->value < end->value ; counter->value++) {
            evaluate(list->values[4]);
          }

          envs.pop();

          return new Nil();
        }
        else if(name == "list") {
          std::vector<Expression*> values;
          for(size_t i = 1 ; i < list->values.size() ; i++) {
            values.push_back(evaluate(list->values[i]));
          }
          return new List(values);
        }
      }
      else if(id == typeid(Symbol)) {
        auto name = (Symbol*)expr;
        auto env = envs.top();
        if(env.find(name->value) == env.end()) {
          throw std::logic_error("undefined variable: " + name->value);
        }
        return env[name->value];
      }

      return expr;
    }

  public:
    Evaluator() {
      envs.push(Environment());
    }

    Expression* evaluate(Expression* expr) {
      return eval_expr(expr);
    }

    template<typename T> T* regard(Expression* expr) {
      if(typeid(*expr) != typeid(T)) {
        throw std::logic_error("illeagl type error: " + expr->lisp_str() + " is not " + std::string(typeid(T).name()));
      }
      return (T*)expr;
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

  Lisp::Evaluator evaluator;
  for(size_t i = 0 ; i < exprs.size() ; i++) {
    exprs[i] = evaluator.evaluate(exprs[i]);
  }

  // fake GC
  for(auto expr : objects) {
    delete expr;
  }

  return 0;
}
