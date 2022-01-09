#pragma once

#include <cassert>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace hdlc::ast {

struct Chip;
struct ChipSignature;
struct ChipBody;
struct Stmt;
struct Expr;
struct Value;
struct Visitor;
struct Package;
struct AssignStmt;
struct CallExpr;
struct RetStmt;
struct RegWrite;
struct RegRead;
struct SliceIdxExpr;
struct SliceJoinExpr;
struct SliceToWireCast;
struct TupleToWireCast;
struct CreateRegisterExpr;

struct WireType;
struct RegisterType;
struct SliceType;
struct TupleType;

struct Node {
  virtual void visit(Visitor &v) = 0;
  virtual ~Node() = default;
};

struct Visitor {
  virtual void visit(Package &pkg) = 0;
  virtual void visit(Chip &chip) = 0;
  virtual void visit(AssignStmt &stmt) = 0;
  virtual void visit(CallExpr &expr) = 0;
  virtual void visit(Value &val) = 0;
  virtual void visit(RetStmt &stmt) = 0;
  virtual void visit(RegWrite &rw) = 0;
  virtual void visit(RegRead &rr) = 0;
  virtual void visit(SliceIdxExpr &e) = 0;
  virtual void visit(SliceJoinExpr &e) = 0;
  virtual void visit(SliceToWireCast &e) = 0;
  virtual void visit(TupleToWireCast &e) = 0;
  virtual void visit(CreateRegisterExpr &e) = 0;
  virtual ~Visitor() = default;
};

struct TypeVisitor {
  virtual void visit(WireType &) = 0;
  virtual void visit(RegisterType &) = 0;
  virtual void visit(SliceType &) = 0;
  virtual void visit(TupleType &) = 0;
  virtual ~TypeVisitor() = default;
};

struct Package : Node {
  std::string name;
  std::vector<std::shared_ptr<Chip>> chips;

  void visit(Visitor &v) override;
};

struct Chip : Node {
  std::string ident;
  std::vector<std::shared_ptr<Value>> inputs;
  std::shared_ptr<TupleType> output_type;
  std::vector<std::shared_ptr<Stmt>> body;

  Chip(std::string ident, std::vector<std::shared_ptr<Value>> inputs,
       std::shared_ptr<TupleType> output_type,
       std::vector<std::shared_ptr<Stmt>> body);

  void visit(Visitor &v) override;
};
struct Type {
  virtual void visit(TypeVisitor &v) = 0;
  virtual ~Type() = default;
};

struct WireType : Type {
  void visit(TypeVisitor &v) override;
};

struct RegisterType : Type {
  void visit(TypeVisitor &v) override;
};

struct SliceType : Type {
  std::shared_ptr<Type> element_type;
  size_t size;

  SliceType(std::shared_ptr<Type> element_type, size_t size);

  void visit(TypeVisitor &v) override;
};

struct TupleType : Type {
  std::vector<std::shared_ptr<Type>> element_types;
  std::vector<std::string> element_names;
  TupleType(std::vector<std::shared_ptr<Type>> types,
            std::vector<std::string> names);

  void visit(TypeVisitor &v) override;
};

struct Stmt : Node {};

struct AssignStmt : Stmt {
  std::vector<std::shared_ptr<Value>> assignees;
  std::shared_ptr<Expr> rhs;

  void visit(Visitor &v) override;
};

struct Expr : Node {
  virtual std::shared_ptr<Type> result_type() = 0;
};

struct CallExpr : Expr {
  std::string chip_name;
  std::vector<std::shared_ptr<Expr>> args;
  std::shared_ptr<Type> res_type;

  CallExpr(std::string name, std::vector<std::shared_ptr<Expr>> args,
           std::shared_ptr<Type> res_type);

  void visit(Visitor &v) override;

  std::shared_ptr<Type> result_type() override;
};

struct Value : Expr {
  std::string ident;
  std::shared_ptr<Type> type;

  Value(std::string name, std::shared_ptr<Type> type);

  void visit(Visitor &v) override;

  std::shared_ptr<Type> result_type() override;
};

struct RetStmt : Stmt {
  std::vector<std::shared_ptr<Expr>> results;

  void visit(Visitor &v) override;
};

struct RegWrite : Stmt {
  std::shared_ptr<Value> reg;
  std::shared_ptr<Expr> rhs;

  explicit RegWrite(std::shared_ptr<Value> reg, std::shared_ptr<Expr> rhs);

  void visit(Visitor &v) override;
};

struct RegRead : Expr {
  std::shared_ptr<Value> reg;

  explicit RegRead(std::shared_ptr<Value> reg);

  void visit(Visitor &v) override;

  std::shared_ptr<Type> result_type() override;
};

struct CastExpr : Expr {
  std::shared_ptr<Expr> expr;

  CastExpr(std::shared_ptr<Expr> expr);
};

struct CreateRegisterExpr : Expr {
  std::shared_ptr<Type> res_type;

  CreateRegisterExpr(std::shared_ptr<Type> res_type);

  void visit(Visitor &v) override;
  std::shared_ptr<Type> result_type() override;
};

struct SliceToWireCast : CastExpr {
  using CastExpr::CastExpr;

  void visit(Visitor &v) override;
  std::shared_ptr<Type> result_type() override;
};

struct TupleToWireCast : CastExpr {
  using CastExpr::CastExpr;

  void visit(Visitor &v) override;
  std::shared_ptr<Type> result_type() override;
};

struct SliceIdxExpr : Expr {
  std::shared_ptr<Expr> slice;
  size_t begin;
  size_t end;

  SliceIdxExpr(std::shared_ptr<Expr> slice, size_t begin, size_t end);

  std::shared_ptr<Type> result_type() override;

  void visit(Visitor &v) override;
};

struct SliceJoinExpr : Expr {
  std::vector<std::shared_ptr<Expr>> values;

  SliceJoinExpr(std::vector<std::shared_ptr<Expr>> values);

  void visit(Visitor &v) override;

  std::shared_ptr<Type> result_type() override;
};

void print_package(std::ostream &out, std::shared_ptr<Package> pkg);
} // namespace hdlc::ast
