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

// cons must be pure list
#define EACH_CONS(var, init) for(Cons* var = regard<Cons>(init) ; typeid(*var) != typeid(Nil) ; var = (Cons*)regard<Cons>(var)->cdr)

// for debug
using std::cout;
using std::endl;

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

    Symbol(std::string avalue) : value(avalue) {}

    std::string lisp_str() { return value; }
  };

  // TODO: RubyみたくExpression*に埋め込みたい
  class Nil : public Expression {
  public:
    Nil() {}

    std::string lisp_str() { return "nil"; }
  };

  class Cons : public Expression {
  public:
    Expression *car, *cdr;

    Cons(Expression* acar, Expression* acdr) : car(acar), cdr(acdr) {}

    std::string lisp_str() {
      return lisp_str_child(true);
    }

    Expression* get(size_t index) {
      if(index == 0) return car;
      else {
        if(typeid(*cdr) == typeid(Cons)) {
          return ((Cons*)cdr)->get(index - 1);
        }
        else {
          //TODO: raise range error
          return nullptr;
        }
      }
    }

    Cons* tail(size_t index) {
      if(index <= 1) return (Cons*)cdr; //TODO: regard使う
      else return (Cons*)tail(index - 1)->cdr;
    }

  private:
    std::string lisp_str_child(bool show_bracket) {
      std::stringstream ss;

      if(show_bracket) ss << '(';

      if(typeid(*car) == typeid(Cons)) {
        ss << ((Cons*)car)->lisp_str_child(true);
      }
      else {
        ss << car->lisp_str();
      }

      if(typeid(*cdr) == typeid(Cons)) {
        ss << " " << ((Cons*)cdr)->lisp_str_child(false);
      }
      else if(typeid(*cdr) != typeid(Nil)){
        ss << " . "; // ドット対
        ss << cdr->lisp_str();
      }

      if(show_bracket) ss << ')';

      return ss.str();
    }
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

      auto first_cons = new Cons(new Nil(), new Nil());
      auto cur_cons   = first_cons;
      size_t count = 0;
      while(!tokens.empty() && cur_token()->type != TOKEN_BRACKET_CLOSE) {
        if(count != 0) {
          cur_cons->cdr = new Cons(new Nil(), new Nil());
          cur_cons = (Cons*)cur_cons->cdr;
        }
        cur_cons->car = parse_expr();

        count++;
      }

      return first_cons;
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
      return c == '!' || ('#' <= c && c <= '\'') || ('*' <= c && c <= '/') ||
            ('<' <= c && c <= '@') || isalpha(c);
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
      if(id == typeid(Cons)) {
        auto list = (Cons*)expr;
        auto name = regard<Symbol>(list->get(0))->value;
        if(name == "print") {
          std::cout << (evaluate(list->get(1)))->lisp_str() << std::endl;
          return new Nil();
        }
        else if(name == "type") {
          return new Symbol(std::string(typeid(*list->get(1)).name()));
        }
        else if(name == "tail") {
          auto arg0  = regard<Cons>(evaluate(list->get(1)));
          auto index = regard<Integer>(evaluate(list->get(2)));
          return arg0->tail(index->value);
        }
        else if(name == "setq") {
          envs.top()[regard<Symbol>(list->get(1))->value] = list->get(2);
          return new Nil();
        }
        else if(name == "atom") {
          auto val = evaluate(list->get(1));
          if(typeid(*val) != typeid(Cons)) return new T();
          else return new Nil();
        }
        else if(name == "+") {
          Integer* sum = new Integer(0);

          EACH_CONS(cc, list->cdr) {
            sum->value += regard<Integer>(evaluate(cc->car))->value;
          }
          return sum;
        }
        else if(name == "=") {
          // TODO: 他の型にも対応させる
          auto x = regard<Integer>(list->get(1));
          auto y = regard<Integer>(list->get(2));

          return (x->value == y->value ? (Expression*)new T() : (Expression*)new Nil());
        }
        else if(name == "let") {
          Environment env;
          auto pairs = regard<Cons>(list->get(1));
          EACH_CONS(cc, pairs) {
            auto kv = regard<Cons>(cc->car);
            env[regard<Symbol>(kv->get(0))->value] = kv->get(1);
          }
          envs.push(env);

          Expression* ret;
          EACH_CONS(cc, list->tail(2)) {
            ret = evaluate(cc->car);
          }

          envs.pop();

          return ret;
        }
        else if(name == "cond") {
          EACH_CONS(cc, list->tail(1)) {
            auto pair = regard<Cons>(cc->get(0));
            if(typeid(*evaluate(pair->get(0))) != typeid(Nil)) {
              return evaluate(pair->get(1));
            }
          }

          return new Nil();
        }
        else if(name == "for") {
          auto counter_name = regard<Symbol>(list->get(1));
          auto start        = regard<Integer>(evaluate(list->get(2)));
          auto end          = regard<Integer>(evaluate(list->get(3)));
          auto body         = list->get(4);

          auto counter      = new Integer(start->value);

          Environment env;
          env[counter_name->value] = counter;
          envs.push(env);

          for(; counter->value < end->value ; counter->value++) {
            evaluate(body);
          }

          envs.pop();

          return new Nil();
        }
        else if(name == "cons") {
          auto car = evaluate(list->get(1));
          auto cdr = evaluate(list->get(2));

          return new Cons(car, cdr);
        }
        else if(name == "list") {
          EACH_CONS(cc, list->cdr) {
            //TODO: 評価する
          }
          return list->cdr;
        }
        else {
          throw std::logic_error("undefined function: " + name);
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
