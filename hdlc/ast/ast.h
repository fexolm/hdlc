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

struct WireType;
struct RegisterType;
struct SliceType;
struct TupleType;

struct Node {
  virtual void visit(Visitor &v) = 0;
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
};

struct TypeVisitor {
  virtual void visit(WireType &) = 0;
  virtual void visit(RegisterType &) = 0;
  virtual void visit(SliceType &) = 0;
  virtual void visit(TupleType &) = 0;
};

struct Package : Node {
  std::string name;
  std::vector<std::shared_ptr<Chip>> chips;

  void visit(Visitor &v) override { v.visit(*this); };
};

struct Chip : Node {
  std::string ident;
  std::vector<std::shared_ptr<Value>> inputs;
  std::shared_ptr<TupleType> output_type;
  std::vector<std::shared_ptr<Stmt>> body;

  Chip() {}

  Chip(std::string ident, std::vector<std::shared_ptr<Value>> inputs,
       std::shared_ptr<TupleType> output_type)
      : ident(ident), inputs(inputs), output_type(output_type) {}

  void visit(Visitor &v) override { v.visit(*this); }
};

struct Type {
  virtual void visit(TypeVisitor &v) = 0;
};

struct WireType : Type {
  virtual void visit(TypeVisitor &v) { v.visit(*this); }
};

struct RegisterType : Type {
  virtual void visit(TypeVisitor &v) { v.visit(*this); }
};

struct SliceType : Type {
  std::shared_ptr<Type> element_type;
  size_t size;

  SliceType(std::shared_ptr<Type> element_type, size_t size)
      : element_type(element_type), size(size) {}

  virtual void visit(TypeVisitor &v) { v.visit(*this); }
};

struct TupleType : Type {
  std::vector<std::shared_ptr<Type>> element_types;
  std::vector<std::string> element_names;
  TupleType(std::vector<std::shared_ptr<Type>> types,
            std::vector<std::string> names)
      : element_types(std::move(types)), element_names(std::move(names)) {}
  virtual void visit(TypeVisitor &v) { v.visit(*this); }
};

struct Stmt : Node {};

struct AssignStmt : Stmt {
  std::vector<std::shared_ptr<Value>> assignees;
  std::shared_ptr<Expr> rhs;

  void visit(Visitor &v) override { v.visit(*this); }
};

struct Expr : Node {
  virtual std::shared_ptr<Type> result_type() = 0;
};

struct CallExpr : Expr {
  std::string chip_name;
  std::vector<std::shared_ptr<Expr>> args;
  std::shared_ptr<Type> res_type;

  CallExpr(std::string name, std::vector<std::shared_ptr<Expr>> args,
           std::shared_ptr<Type> res_type)
      : chip_name(name), args(args), res_type(res_type) {}

  void visit(Visitor &v) override { v.visit(*this); }

  virtual std::shared_ptr<Type> result_type() { return res_type; }
};

struct Value : Expr {
  std::string ident;
  std::shared_ptr<Type> type;

  Value(std::string name, std::shared_ptr<Type> type)
      : ident(name), type(type) {}

  void visit(Visitor &v) override { v.visit(*this); }

  virtual std::shared_ptr<Type> result_type() { return type; }
};

struct RetStmt : Stmt {
  std::vector<std::shared_ptr<Expr>> results;

  void visit(Visitor &v) override { v.visit(*this); }
};

struct RegWrite : Stmt {
  std::shared_ptr<Value> reg;
  std::shared_ptr<Expr> rhs;

  explicit RegWrite(std::shared_ptr<Value> reg, std::shared_ptr<Expr> rhs)
      : reg(reg), rhs(rhs) {}

  void visit(Visitor &v) override { v.visit(*this); }
};

struct RegRead : Expr {
  std::shared_ptr<Value> reg;

  explicit RegRead(std::shared_ptr<Value> reg) : reg(reg) {}

  void visit(Visitor &v) override { v.visit(*this); }

  virtual std::shared_ptr<Type> result_type() {
    assert(false);
    return nullptr;
  }
};

struct Printer : Visitor {
  std::ostream &out;

  Printer(std::ostream &out) : out(out) {}
  void visit(Package &pkg) {
    out << "Package: " << pkg.name << "\n\n";
    for (auto &c : pkg.chips) {
      c->visit(*this);
    }
  }
  void visit(Chip &chip) {
    out << "chip " << chip.ident << " (";
    for (auto &i : chip.inputs) {
      i->visit(*this);
      out << ", ";
    }
    out << ") ";

    for (auto &name : chip.output_type->element_names) {
      out << name << ", ";
    }
    out << "{" << std::endl;

    for (auto &s : chip.body) {
      s->visit(*this);
      out << std::endl;
    }
    out << "}"
        << "\n\n";
  }

  void visit(Value &val) { out << val.ident; }
  void visit(AssignStmt &stmt) {
    out << "    ";
    for (auto &v : stmt.assignees) {
      v->visit(*this);
      out << ", ";
    }
    out << ":= ";

    stmt.rhs->visit(*this);
  }
  void visit(CallExpr &expr) {
    out << expr.chip_name << "(";
    for (auto &arg : expr.args) {
      arg->visit(*this);
      out << ", ";
    }
    out << ")";
  }
  void visit(RetStmt &stmt) {
    out << "    return ";
    for (auto &v : stmt.results) {
      v->visit(*this);
      out << ", ";
    }
  }

  void visit(RegWrite &rw) {
    out << "    ";
    rw.reg->visit(*this);
    out << " <- ";
    rw.rhs->visit(*this);
  }

  void visit(RegRead &rr) {
    out << "<- ";
    rr.reg->visit(*this);
  }
};
} // namespace hdlc::ast