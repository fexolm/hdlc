#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ast {

struct Chip;
struct ChipSignature;
struct ChipBody;
struct Stmt;
struct Expr;
struct Value;
struct Type;

struct Package {
  std::string name;
  std::vector<std::shared_ptr<Chip>> chips;
};

struct Chip {
  std::string ident;
  std::vector<std::shared_ptr<Value>> inputs;
  std::vector<std::shared_ptr<Type>> outputs;
  std::vector<std::shared_ptr<Stmt>> body;
};

struct Type {
  std::string name;

  explicit Type(const std::string &name) : name(name) {}
};

struct Stmt {};

struct AssignStmt : public Stmt {
  std::vector<std::shared_ptr<Value>> assignees;
  std::shared_ptr<Expr> rhs;
};

struct Expr {};

struct CallExpr : public Expr {
  std::string chip_name;
  std::vector<std::shared_ptr<Expr>> args;

  CallExpr(std::string name, std::vector<std::shared_ptr<Expr>> args)
      : chip_name(name), args(args) {}
};

struct Value : public Expr {
  std::string ident;
  std::shared_ptr<Type> type;

  Value(std::string name, std::shared_ptr<Type> type)
      : ident(name), type(type) {}

  Value() {}
};

struct RetStmt : public Stmt {
  std::vector<std::shared_ptr<Expr>> results;
};

} // namespace ast