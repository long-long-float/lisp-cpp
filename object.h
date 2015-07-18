#pragma once

#include "gc.h"

#include <string>
#include <cstdlib>
#include <sstream>

namespace Lisp {
  class Environment;

  class Object : public GCObject {
  public:
    virtual std::string lisp_str() = 0;
  };

  class String : public Object {
  public:
    std::string value;

    String(std::string &avalue) : value(avalue) {}

    std::string lisp_str();
  };

  class Integer : public Object {
  public:
    long value;

    Integer(std::string &avalue)  : value(std::atol(avalue.c_str())) {}
    Integer(long avalue) : value(avalue) {}

    std::string lisp_str();
  };

  class Symbol : public Object {
  public:
    std::string value;

    Symbol(std::string avalue) : value(avalue) {}

    std::string lisp_str();
  };

  // TODO: RubyみたくObject*に埋め込みたい
  class Nil : public Object {
  public:
    Nil() {}

    std::string lisp_str();
  };

  class T : public Object {
  public:
    T() {}

    std::string lisp_str();
  };

  class Cons : public Object {
  public:
    Object *car, *cdr;

    Cons(Object* acar, Object* acdr) : car(acar), cdr(acdr) {}

    void mark();

    std::string lisp_str();

    Object* get(size_t index);
    int find(Object *item);
    Cons* tail(size_t index);

  private:
    std::string lisp_str_child(bool show_bracket);
  };

  class Lambda : public Object {
  public:
    Cons *args, *body;
    Environment *lexical_parent;

    Lambda(Cons *aargs, Cons *abody, Environment* alexical_parent)
      : args(aargs), body(abody), lexical_parent(alexical_parent) {}

    std::string lisp_str();

    void mark();
  };

  class Macro : public Object {
    Cons *args, *body;

    Object* expand_rec(Cons* src_args, Object* cur_body);

    public:
    Macro(Cons *aargs, Cons *abody) : args(aargs), body(abody) {}

    Object* expand(Cons* src_args);

    void mark();

    std::string lisp_str();
 };

}
