#pragma once

#include "gc.h"
#include <string>

namespace Lisp {
  enum TokenType{
    TOKEN_BRACKET_OPEN,
    TOKEN_BRACKET_CLOSE,
    TOKEN_SYMBOL,
    TOKEN_STRING,
    TOKEN_INTEGER,
    TOKEN_NIL,
    TOKEN_T,
  };

  class Object;

  class Token : public GCObject {
  public:
    TokenType type;
    std::string value;

    Token(TokenType atype) : type(atype), value(std::string()) {}
    Token(TokenType atype, std::string avalue) : type(atype), value(avalue) {}

    std::string str();
  };
}
