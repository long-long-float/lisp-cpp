#include <iostream>
#include <fstream>
#include <iterator>
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

#include <dlfcn.h>

#include "lisp.h"

#define PRINT_LINE (std::cout << "line: " << __LINE__ << std::endl)

// cons must be pure list
#define EACH_CONS(var, init) for(Cons* var = regard<Cons>(init) ; typeid(*var) != typeid(Nil) ; var = (Cons*)regard<Cons>(var)->cdr)

// for debug
using std::cout;
using std::endl;

namespace Lisp {
  class Error : public std::logic_error {
  public:
    Error(std::string msg, Location loc) : std::logic_error(msg + " @ " + loc.str()) {}
  };

  class NameError : public Error {
  public:
    NameError(Symbol *sym) : Error("undefined local variable " + sym->value, sym->loc) {}
  };

  class TypeError : public Error  {
  public:
    TypeError(Object* obj, std::string expected_type) :
      Error(obj->lisp_str() + " is not " + expected_type, obj->loc) {}
  };

  class Parser {
  public:
    std::vector<Object*> parse(const std::string &code) {
      tokens = tokenize(code);

      if(false) { // NOTE: for debug
        for(auto tok : tokens) {
          std::cout << tok->str() << std::endl;
        }
      }

      std::vector<Object*> exprs;
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

    Cons* parse_list() {
      consume_token();
      if(cur_token()->type != TOKEN_SYMBOL) {
        //TODO: raise an error
      }

      auto first_cons = new Cons(new Nil(), new Nil(), cur_token()->loc);
      auto cur_cons   = first_cons;
      size_t count = 0;
      while(true) {
        if(tokens.empty()) {
          throw std::logic_error("unexpected end of code : expected ')'");
        }
        if(cur_token()->type == TOKEN_BRACKET_CLOSE) break;

        if(count != 0) {
          cur_cons->cdr = new Cons(new Nil(), new Nil(), cur_token()->loc);
          cur_cons = (Cons*)cur_cons->cdr;
        }
        cur_cons->car = parse_expr();

        count++;
      }

      return first_cons;
    }

    Object* parse_expr() {
      auto ctoken = cur_token();
      auto ttype = ctoken->type;
      if(ttype == TOKEN_BRACKET_OPEN) {
        auto ret = parse_list();
        consume_token();
        return ret;
      }

      consume_token();
      switch(ttype) {
        case TOKEN_BRACKET_OPEN: return nullptr; //not reached
        case TOKEN_SYMBOL:
          return new Symbol(ctoken->value, ctoken->loc);
        case TOKEN_STRING:
          return new String(ctoken->value, ctoken->loc);
        case TOKEN_INTEGER:
          return new Integer(ctoken->value, ctoken->loc);
        case TOKEN_NIL:
          return new Nil(ctoken->loc);
        case TOKEN_T:
          return new T(ctoken->loc);
        default:
          throw std::logic_error("unknown token: " + std::to_string(cur_token()->type));
      }
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

      int lineno = 1, colno = 0;

      for(size_t i = 0 ; i < code.size() - 1 ; i++) { //TODO: -1を修正(EOFっぽい?)
        char ch = code[i];

        size_t ti = i; // for calcurating lineno

        if(ch == ';') { // comment
          while(code[i] != '\n' && i < code.size()) {
            i++;
          }
        }
        else if(ch == '(')
          tokens.push_back(new Token(TOKEN_BRACKET_OPEN, Location(lineno, colno)));
        else if(ch == ')')
          tokens.push_back(new Token(TOKEN_BRACKET_CLOSE, Location(lineno, colno)));
        else if(ch == '"') { // string
          i++;

          size_t token_len = 0;
          while(code[i + token_len] != '"') {
            token_len++;
            if(i + token_len >= code.size()) {
              //TODO: raise an error
            }
          }
          tokens.push_back(new Token(TOKEN_STRING, code.substr(i, token_len), Location(lineno, colno)));
          i += token_len;
        }
        else if(ch == ' ')
          ; // skip
        else if(ch == '\n') {
          lineno++;
          colno = 0;
        }
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
          tokens.push_back(new Token(TOKEN_INTEGER, token_val, Location(lineno, colno)));

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
            tokens.push_back(new Token(TOKEN_SYMBOL, token_val, Location(lineno, colno)));
          else
            tokens.push_back(new Token(token_type, Location(lineno, colno)));

          i += token_len - 1;
        }

        colno += i - ti + 1;
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
          auto val = evaluate(list->get(2));
          cur_env->set(regard<Symbol>(list->get(1))->value, val);
          return val;
        }
        else if(name == "defmacro") {
          cur_env->set(regard<Symbol>(list->get(1))->value,
            new Macro(regard<Cons>(list->get(2)), regard<Cons>(list->get(3))));
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
        else if(name == "*") {
          Integer* prod = new Integer(1);

          EACH_CONS(cc, list->cdr) {
            prod->value *= regard<Integer>(evaluate(cc->car))->value;
          }
          return prod;
        }
        else if(name == "=") {
          // TODO: 他の型にも対応させる
          auto x = regard<Integer>(evaluate(list->get(1)));
          auto y = regard<Integer>(evaluate(list->get(2)));

          return (x->value == y->value ? (Object*)new T() : (Object*)new Nil());
        }
        else if(name == ">") {
          auto x = regard<Integer>(evaluate(list->get(1)));
          auto y = regard<Integer>(evaluate(list->get(2)));

          return (x->value > y->value ? (Object*)new T() : (Object*)new Nil());
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
          return new Lambda(regard<Cons>(list->get(1)), list->tail(2), cur_env);
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
          mark();
          sweep();
          return new Nil();
        }
        else if(name == "require") {
          // load dynamic module
          auto modname = "plugin/" + regard<String>(evaluate(list->get(1)))->value + ".so";
          auto handle = dlopen(modname.c_str(), RTLD_LAZY);
          if(!handle) {
            throw std::logic_error("can't load dynamic module: " + modname);
          }

          dlerror();

          auto init = (void(*)(void))dlsym(handle, "slisp_init");

          char *error = dlerror();
          if(error) {
            throw std::logic_error(error);
          }

          (*init)();

          return new Nil();
        }
        else {
          auto obj = evaluate(list->get(0));
          if(typeid(*obj) == typeid(Lambda)) {
            Lambda* lambda = (Lambda*)obj;

            Environment *env = new Environment();
            size_t index = 1;
            EACH_CONS(cc, lambda->args) {
              if(typeid(*cc->car) == typeid(Nil)) break; //TODO なんとかする
              auto name = regard<Symbol>(cc->car)->value;
              env->set(name, evaluate(list->get(index)));

              index++;
            }
            env->set_lexical_parent(lambda->lexical_parent);

            cur_env = cur_env->down_env(env);

            Object* ret;
            EACH_CONS(cc, lambda->body) {
              ret = evaluate(cc->car);
            }

            cur_env = cur_env->up_env();
            return ret;
          }
          else if(typeid(*obj) == typeid(Macro)) {
            Macro* mac = (Macro*)obj;

            auto expanded = mac->expand(list->tail(1));
            return evaluate(expanded);
          }
          else {
            throw std::logic_error("undefined function: " + name);
          }
        }
      }
      else if(id == typeid(Symbol)) {
        auto name = (Symbol*)obj;
        auto val = cur_env->get(name->value);
        if(val != nullptr) return val;

        throw NameError(name);
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

    Object* evaluate(std::vector<Object*> exprs) {
      Object *ret;
      for(auto &expr : exprs) {
        ret = evaluate(expr);
      }
      return ret;
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
        throw TypeError(expr, std::string(typeid(T).name()));
      }
      return (T*)expr;
    }
  };

  std::vector<Object*> parse(std::string &code) {
    Parser p;
    return p.parse(code);
  }

  void clean_up() {
    for(auto obj : objects) {
      delete obj;
    }
  }
}

int main() {
  using namespace std;

  Lisp::Evaluator evaluator;

  // load standard module
  ifstream stdmod_ifs("std.lisp");
  if(stdmod_ifs.fail()) {
    cerr << "failed to load 'std.lisp'!" << endl;
    Lisp::clean_up();
    return 1;
  }
  string stdmod((istreambuf_iterator<char>(stdmod_ifs)), istreambuf_iterator<char>());
  evaluator.evaluate(Lisp::parse(stdmod));

  string code((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());
  evaluator.evaluate(Lisp::parse(code));

  Lisp::clean_up();

  return 0;
}
