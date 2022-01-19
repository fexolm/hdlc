#include "ast.h"

namespace hdlc::ast {
void Package::visit(Visitor &v) { v.visit(*this); }

Chip::Chip(std::string ident, std::vector<std::shared_ptr<Value>> inputs,
           std::shared_ptr<TupleType> output_type,
           std::vector<std::shared_ptr<Stmt>> body)
    : ident(ident), inputs(inputs), output_type(output_type),
      body(std::move(body)) {}

void Chip::visit(Visitor &v) { v.visit(*this); }

void WireType::visit(TypeVisitor &v) { v.visit(*this); }

void RegisterType::visit(TypeVisitor &v) { v.visit(*this); }

SliceType::SliceType(std::shared_ptr<Type> element_type, size_t size)
    : element_type(element_type), size(size) {}

void SliceType::visit(TypeVisitor &v) { v.visit(*this); }

TupleType::TupleType(std::vector<std::shared_ptr<Type>> types,
                     std::vector<std::string> names)
    : element_types(std::move(types)), element_names(std::move(names)) {}
void TupleType::visit(TypeVisitor &v) { v.visit(*this); }

void AssignStmt::visit(Visitor &v) { v.visit(*this); }

CallExpr::CallExpr(std::string name, std::vector<std::shared_ptr<Expr>> args,
                   std::shared_ptr<Type> res_type)
    : chip_name(name), args(args), res_type(res_type) {}

void CallExpr::visit(Visitor &v) { v.visit(*this); }

std::shared_ptr<Type> CallExpr::result_type() { return res_type; }

Value::Value(std::string name, std::shared_ptr<Type> type)
    : ident(name), type(type) {}

void Value::visit(Visitor &v) { v.visit(*this); }

std::shared_ptr<Type> Value::result_type() { return type; }

void RetStmt::visit(Visitor &v) { v.visit(*this); }

RegWrite::RegWrite(std::shared_ptr<Value> reg, std::shared_ptr<Expr> rhs)
    : reg(reg), rhs(rhs) {}

void RegWrite::visit(Visitor &v) { v.visit(*this); }

RegRead::RegRead(std::shared_ptr<Value> reg) : reg(reg) {}

void RegRead::visit(Visitor &v) { v.visit(*this); }

std::shared_ptr<Type> RegRead::result_type() {
  if (auto t = std::dynamic_pointer_cast<SliceType>(reg->result_type())) {
    return std::make_shared<SliceType>(std::make_shared<WireType>(), t->size);
  }
  return std::make_shared<WireType>();
}

CastExpr::CastExpr(std::shared_ptr<Expr> expr) : expr(expr) {}

void SliceToWireCast::visit(Visitor &v) { v.visit(*this); }

std::shared_ptr<Type> SliceToWireCast::result_type() {
  return std::make_shared<WireType>();
}

void TupleToWireCast::visit(Visitor &v) { v.visit(*this); }

std::shared_ptr<Type> TupleToWireCast::result_type() {
  return std::make_shared<WireType>();
}

SliceIdxExpr::SliceIdxExpr(std::shared_ptr<Expr> slice, size_t begin,
                           size_t end)
    : slice(slice), begin(begin), end(end) {}

std::shared_ptr<Type> SliceIdxExpr::result_type() {
  assert(end - begin >= 1);
  auto slice_type = std::dynamic_pointer_cast<SliceType>(slice->result_type());
  assert(slice_type->size >= end - begin);
  return std::make_shared<SliceType>(slice_type->element_type, end - begin);
}

void SliceIdxExpr::visit(Visitor &v) { v.visit(*this); }

SliceJoinExpr::SliceJoinExpr(std::vector<std::shared_ptr<Expr>> values)
    : values(std::move(values)) {}

void SliceJoinExpr::visit(Visitor &v) { v.visit(*this); }

std::shared_ptr<Type> SliceJoinExpr::result_type() {
  // TODO: Check if all types are equal
  return std::make_shared<SliceType>(std::make_shared<WireType>(),
                                     values.size());
}

CreateRegisterExpr::CreateRegisterExpr(std::shared_ptr<Type> res_type)
    : res_type(res_type) {}

void CreateRegisterExpr::visit(Visitor &v) { v.visit(*this); }
std::shared_ptr<Type> CreateRegisterExpr::result_type() { return res_type; }

struct Printer : Visitor {
private:
  std::ostream &out;

public:
  Printer(std::ostream &out) : out(out) {}

  void visit(Package &pkg) override {
    out << "Package: " << pkg.name << "\n\n";
    for (auto &c : pkg.chips) {
      c->visit(*this);
    }
  }

  void visit(Chip &chip) override {
    if (chip.ident == "Nand") {
      return;
    }

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

  void visit(Value &val) override { out << val.ident; }

  void visit(AssignStmt &stmt) override {
    out << "    ";
    for (auto &v : stmt.assignees) {
      v->visit(*this);
      out << ", ";
    }
    out << ":= ";

    stmt.rhs->visit(*this);
  }

  void visit(CallExpr &expr) override {
    out << expr.chip_name << "(";
    for (auto &arg : expr.args) {
      arg->visit(*this);
      out << ", ";
    }
    out << ")";
  }

  void visit(RetStmt &stmt) override {
    out << "    return ";
    for (auto &v : stmt.results) {
      v->visit(*this);
      out << ", ";
    }
  }

  void visit(RegWrite &rw) override {
    out << "    ";
    rw.reg->visit(*this);
    out << " <- ";
    rw.rhs->visit(*this);
  }

  void visit(RegRead &rr) override {
    out << "<- ";
    rr.reg->visit(*this);
  }

  void visit(SliceIdxExpr &e) override {
    e.slice->visit(*this);
    out << "[" << e.begin << ":" << e.end << "]";
  }

  void visit(SliceJoinExpr &e) override {
    out << "[";
    for (auto &se : e.values) {
      se->visit(*this);
      out << ",";
    }
    out << "]";
  }

  void visit(SliceToWireCast &e) override {
    out << "*";
    e.expr->visit(*this);
  }
  void visit(TupleToWireCast &e) override {
    out << "*";
    e.expr->visit(*this);
  }

  void visit(CreateRegisterExpr &) override { out << "Register()"; }
};

void print_package(std::ostream &out, std::shared_ptr<Package> pkg) {
  Printer p(out);
  p.visit(*pkg);
}

} // namespace hdlc::ast