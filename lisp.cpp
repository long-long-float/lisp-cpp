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
  TOKEN_T,
};

enum ExpressionType {
  EXPRESSION_LIST,
  EXPRESSION_SYMBOL = TOKEN_SYMBOL,
  EXPRESSION_STRING,
  EXPRESSION_INTEGER,
  EXPRESSION_NIL = TOKEN_NIL,
  EXPRESSION_T,
};

namespace Lisp {
  class GCObject;
}

// memory allocated exprs
std::list<Lisp::GCObject*> objects;

namespace Lisp {
  // GC target object
  class GCObject {
  public:
    bool mark_flag;

    GCObject() {
      objects.push_back(this);
    }

    virtual ~GCObject() {}

    virtual void mark() {
      mark_flag = true;
    }
  };

  class Object : public GCObject {
  public:
    virtual std::string lisp_str() = 0;
  };

  class String : public Object {
  public:
    std::string value;

    String(std::string &avalue) : value(avalue) {}

    std::string lisp_str() { return '"' + value + '"'; }
  };

  class Integer : public Object {
  public:
    long value;

    Integer(std::string &avalue)  : value(std::atol(avalue.c_str())) {}
    Integer(long avalue) : value(avalue) {}

    std::string lisp_str() { return std::to_string(value); }
  };

  class Symbol : public Object {
  public:
    std::string value;

    Symbol(std::string avalue) : value(avalue) {}

    std::string lisp_str() { return value; }
  };

  // TODO: RubyみたくObject*に埋め込みたい
  class Nil : public Object {
  public:
    Nil() {}

    std::string lisp_str() { return "nil"; }
  };

  class Cons : public Object {
  public:
    Object *car, *cdr;

    Cons(Object* acar, Object* acdr) : car(acar), cdr(acdr) {}

    void mark() {
      Object::mark();
      car->mark(); cdr->mark();
    }

    std::string lisp_str() {
      return lisp_str_child(true);
    }

    Object* get(size_t index) {
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

  class T : public Object {
  public:
    T() {}

    std::string lisp_str() { return "T"; }
  };

  class Lambda : public Object {
  public:
    Cons *args, *body;

    Lambda(Cons *aargs, Cons *abody) : args(aargs), body(abody) {}

    std::string lisp_str() {
      std::stringstream ss;
      ss << "(lambda " << args->lisp_str() << " " << body->lisp_str() << ")";
      return ss.str();
    }
  };

  class Token : public GCObject {
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

  class Expression : public GCObject {
  public:
    ExpressionType type;
    std::string value;

    Expression(ExpressionType atype) : type(atype) {}
    Expression(ExpressionType atype, std::string avalue) : type(atype), value(avalue) {}

    virtual Object* to_obj() {
      switch(type) {
        case EXPRESSION_LIST:
          // this must be converted in Evaluator.evaluate
          break;
        case EXPRESSION_STRING:
          return new String(value);
        case EXPRESSION_NIL:
          return new Nil();
        case EXPRESSION_T:
          return new T();
        case EXPRESSION_SYMBOL:
          return new Symbol(value);
        case EXPRESSION_INTEGER:
          return new Integer(value);
     }
     throw std::logic_error("unknown expression: " + std::to_string(type));
    }

    std::string str() { return std::to_string(type); }
  };

  class List : public Expression {
  public:
    std::vector<Expression*> values;

    List() : Expression(EXPRESSION_LIST) {}

    Object* to_obj() {
      auto first_cons = new Cons(new Nil(), new Nil());
      auto cur_cons   = first_cons;
      size_t count = 0;
      for(auto expr : values) {
        if(count != 0) {
          cur_cons->cdr = new Cons(new Nil(), new Nil());
          cur_cons = (Cons*)cur_cons->cdr;
        }
        cur_cons->car = expr->to_obj();

        count++;
      }

      return first_cons;
    };

    Expression* get(size_t index) {
      return values[index];
    }
  };

  class Environment : public GCObject {
    typedef std::string key;

    std::map<key, Object*> locals;

    Environment *parent, *child;

    bool exists_local(key &name) {
      return locals.find(name) != locals.end();
    }
  public:
    Environment() : parent(nullptr), child(nullptr) {}

    void set(key &name, Object* val) {
      if(!exists_local(name) && parent) parent->set(name, val);
      else locals[name] = val;
    }

    Object* get(key &name) {
      if(exists_local(name)) {
        return locals[name];
      }
      return parent ? parent->get(name) : nullptr;
    }

    Environment* down_env(Environment *new_env) {
      child = new_env;
      new_env->parent = this;
      return new_env;
    }

    Environment* up_env() {
      auto parent_env = parent;
      parent_env->child = nullptr;
      return parent_env;
    }

    void mark() {
      for(auto& kv : locals) {
        kv.second->mark();
      }
      if(child) child->mark();
    }
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
      if(!tokens.empty()) tokens.pop_front();
    }

    List* parse_list() {
      consume_token();
      if(cur_token()->type != TOKEN_SYMBOL) {
        //TODO: raise an error
      }

      List* list = new List();
      while(!tokens.empty() && cur_token()->type != TOKEN_BRACKET_CLOSE) {
        list->values.push_back(parse_expr());
      }

      return list;

      /*
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
      */
    }

    Expression* parse_expr() {
      Expression* ret;
      auto ttype = cur_token()->type;
      switch(ttype) {
        case TOKEN_BRACKET_OPEN:
          ret = parse_list();
          break;
        case TOKEN_SYMBOL: // == EXPRESSION_SYMBOL
        case TOKEN_STRING:
        case TOKEN_INTEGER:
          ret = new Expression((ExpressionType)ttype, cur_token()->value);
          break;
        case TOKEN_NIL: // == EXPRESSION_NIL
        case TOKEN_T:
          ret = new Expression((ExpressionType)ttype);
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
        if(ch == ';') { // comment
          while(code[i] != '\n' && i < code.size()) {
            i++;
          }
        }
        else if(ch == '(')
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
        else if((ch == '-' && is_number(code[i + 1])) || is_number(ch)) {
          size_t token_len = 0;

          if(ch == '-') token_len++;

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
          else if(token_val == "t")
            token_type = TOKEN_T;
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
    Environment *root_env, *cur_env;

    Object* eval_expr(Object* obj) {
      std::type_info const & id = typeid(*obj);
      if(id == typeid(Cons)) {
        auto list = (Cons*)obj;
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
          cur_env->set(regard<Symbol>(list->get(1))->value, evaluate(list->get(2)));
          return new Nil();
        }
        else if(name == "defun") {
          cur_env->set(regard<Symbol>(list->get(1))->value,
            new Lambda(regard<Cons>(list->get(2)), list->tail(3)));
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
        else if(name == "-") {
          Integer* sub = regard<Integer>(evaluate(list->get(1)));

          EACH_CONS(cc, list->tail(2)) {
            sub->value -= regard<Integer>(evaluate(cc->car))->value;
          }
          return sub;
        }
        else if(name == "=") {
          // TODO: 他の型にも対応させる
          auto x = regard<Integer>(evaluate(list->get(1)));
          auto y = regard<Integer>(evaluate(list->get(2)));

          return (x->value == y->value ? (Object*)new T() : (Object*)new Nil());
        }
        else if(name == "mod") {
          auto x = regard<Integer>(evaluate(list->get(1)));
          auto y = regard<Integer>(evaluate(list->get(2)));

          return new Integer(x->value % y->value);
        }
        else if(name == "let") {
          Environment* env = new Environment();
          auto pairs = regard<Cons>(list->get(1));
          EACH_CONS(cc, pairs) {
            auto kv = regard<Cons>(cc->car);
            env->set(regard<Symbol>(kv->get(0))->value, kv->get(1));
          }
          cur_env = cur_env->down_env(env);

          Object* ret;
          EACH_CONS(cc, list->tail(2)) {
            ret = evaluate(cc->car);
          }

          cur_env = cur_env->up_env();

          return ret;
        }
        else if(name == "lambda") {
          return new Lambda(regard<Cons>(list->get(1)), list->tail(2));
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

          auto counter      = new Integer(start->value);

          Environment *env = new Environment();
          env->set(counter_name->value, counter);

          cur_env = cur_env->down_env(env);

          for(; counter->value < end->value ; counter->value++) {
            EACH_CONS(cc, list->tail(4)) {
              evaluate(cc->get(0));
            }
          }

          cur_env = cur_env->up_env();

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
        else if(name == "number-of-objects") {
          return new Integer(objects.size());
        }
        else if(name == "gc") {
          mark(); sweep();
          return new Nil();
        }
        else {
          try {
            Lambda* lambda = regard<Lambda>(evaluate(list->get(0)));

            Environment *env = new Environment();
            size_t index = 1;
            EACH_CONS(cc, lambda->args) {
              auto name = regard<Symbol>(cc->car)->value;
              env->set(name, evaluate(list->get(index)));

              index++;
            }

            cur_env = cur_env->down_env(env);

            Object* ret;
            EACH_CONS(cc, lambda->body) {
              ret = evaluate(cc->car);
            }

            cur_env = cur_env->up_env();
            return ret;
          }
          catch (const std::logic_error &e) {
            // first element isn't lambda
            throw std::logic_error("undefined function: " + name);
          }
        }
      }
      else if(id == typeid(Symbol)) {
        auto name = (Symbol*)obj;
        auto val = cur_env->get(name->value);
        if(val != nullptr) return val;

        throw std::logic_error("undefined variable: " + name->value);
      }

      return obj;
    }

  public:
    Evaluator() {
      root_env = cur_env = new Environment();
    }

    Object* evaluate(Object* expr) {
      return eval_expr(expr);
    }

    void mark() {
      root_env->mark();
    }

    void sweep() {
      for(auto itr = objects.begin() ; itr != objects.end() ; ) {
        auto obj = *itr;
        if(!obj->mark_flag) {
          delete obj;
          itr = objects.erase(itr);
          continue;
        }
        else {
          obj->mark_flag = false;
        }
        itr++;
      }
    }

    template<typename T> T* regard(Object* expr) {
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
  vector<Lisp::Object*> exprs2(exprs.size());
  for(size_t i = 0 ; i < exprs.size() ; i++) {
    exprs2[i] = exprs[i]->to_obj();
  }

  Lisp::Evaluator evaluator;
  for(size_t i = 0 ; i < exprs.size() ; i++) {
    evaluator.evaluate(exprs2[i]);
  }

  // clean up
  for(auto expr : objects) {
    delete expr;
  }

  return 0;
}
