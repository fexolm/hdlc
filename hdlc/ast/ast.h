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
  virtual void visit(SliceIdxExpr &e) = 0;
  virtual void visit(SliceJoinExpr &e) = 0;
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

struct SliceIdxExpr : Expr {
  std::shared_ptr<Expr> slice;
  size_t begin;
  size_t end;

  SliceIdxExpr(std::shared_ptr<Expr> slice, size_t begin, size_t end)
      : slice(slice), begin(begin), end(end) {}

  std::shared_ptr<Type> result_type() override {
    assert(end - begin >= 1);
    auto slice_type =
        std::dynamic_pointer_cast<SliceType>(slice->result_type());
    assert(slice_type->size >= end - begin);
    return std::make_shared<SliceType>(slice_type->element_type, end - begin);
  }

  void visit(Visitor &v) override { v.visit(*this); }
};

struct SliceJoinExpr : Expr {
  std::vector<std::shared_ptr<Expr>> values;

  SliceJoinExpr(std::vector<std::shared_ptr<Expr>> values)
      : values(std::move(values)) {}

  void visit(Visitor &v) override { v.visit(*this); }

  std::shared_ptr<Type> result_type() override {
    // TODO: Check if all types are equal
    return std::make_shared<SliceType>(values[0]->result_type(), values.size());
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
      if (auto st = std::dynamic_pointer_cast<SliceType>(i->type)) {
        out << "[" << st->size << "]";
      }
      out << ", ";
    }
    out << ") ";

    for (size_t i = 0; i < chip.output_type->element_types.size(); ++i) {
      auto name = chip.output_type->element_names[i];
      auto type = chip.output_type->element_types[i];

      out << name;

      if (auto st = std::dynamic_pointer_cast<SliceType>(type)) {
        out << "[" << st->size << "]";
      }

      out << ", ";
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

  void visit(SliceIdxExpr &e) override {
    e.slice->visit(*this);
    out << "[" << e.begin << ":" << e.end << "]";
  }

  void visit(SliceJoinExpr &e) override {
    out << "[";
    for (auto se : e.values) {
      se->visit(*this);
      out << ",";
    }
    out << "]";
  }
};
} // namespace hdlc::ast
