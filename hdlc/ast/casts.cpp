#include "transforms.h"
#include <algorithm>
#include <cassert>

namespace hdlc::ast {

struct InsertCastsVisitor : Visitor {
  Package *cur_pkg;
  Chip *cur_chip;

  void visit(Package &pkg) {
    cur_pkg = &pkg;
    for (auto &c : pkg.chips) {
      c->visit(*this);
    }
  }

  void visit(Chip &chip) {
    if (chip.ident == "Nand") {
      return;
    }

    cur_chip = &chip;
    for (auto &s : chip.body) {
      s->visit(*this);
    }
  }

  void visit(AssignStmt &stmt) {
    if (std::dynamic_pointer_cast<CallExpr>(stmt.rhs)) {
      // Correctly propogate types at first as we don't do this at parsing time
      auto args_count = stmt.assignees.size();
      // TODO: Check
      auto tuple_type =
          std::static_pointer_cast<TupleType>(stmt.rhs->result_type());

      assert(tuple_type->element_types.size() == args_count);

      for (int i = 0; i < args_count; i++) {
        auto type = tuple_type->element_types[i];
        stmt.assignees[i]->type = type;
      }
    }

    if (std::dynamic_pointer_cast<CreateRegisterExpr>(stmt.rhs)) {
      stmt.assignees[0]->type = stmt.rhs->result_type();
    }
    stmt.rhs->visit(*this);
  }

  std::shared_ptr<Expr> cast(std::shared_ptr<Expr> expr,
                             std::shared_ptr<Type> type) {
    if (auto wt = std::dynamic_pointer_cast<WireType>(type)) {
      if (std::dynamic_pointer_cast<WireType>(expr->result_type())) {
        return expr;
      }
      if (auto st = std::dynamic_pointer_cast<SliceType>(expr->result_type())) {
        assert(st->size = 1);
        return std::make_shared<SliceToWireCast>(expr);
      }
      auto tt = std::dynamic_pointer_cast<TupleType>(expr->result_type());
      assert(tt);
      assert(tt->element_types.size() == 1);

      return std::make_shared<TupleToWireCast>(expr);
    }
    if (auto st = std::dynamic_pointer_cast<SliceType>(type)) {
      if (std::dynamic_pointer_cast<SliceType>(expr->result_type())) {
        return expr;
      }
    }
    assert(false);
    return nullptr;
  }

  void visit(CallExpr &expr) {
    for (auto &arg : expr.args) {
      arg->visit(*this);
    }

    auto chip_iter =
        std::find_if(cur_pkg->chips.begin(), cur_pkg->chips.end(),
                     [&expr](auto c) { return c->ident == expr.chip_name; });

    // TODO: throw
    assert(chip_iter != cur_pkg->chips.end());

    for (int i = 0; i < expr.args.size(); ++i) {
      auto type = (*chip_iter)->inputs[i]->type;
      expr.args[i] = cast(expr.args[i], type);
    }
  }

  void visit(RetStmt &stmt) {
    for (auto &v : stmt.results) {
      v->visit(*this);
    }
    assert(stmt.results.size() == cur_chip->output_type->element_types.size());
    for (int i = 0; i < stmt.results.size(); ++i) {
      auto type = cur_chip->output_type->element_types[i];
      stmt.results[i] = cast(stmt.results[i], type);
    }
  }

  void visit(SliceJoinExpr &e) override {
    for (auto se : e.values) {
      se->visit(*this);
    }
    auto slice_type = std::static_pointer_cast<SliceType>(e.result_type());

    for (int i = 0; i < e.values.size(); ++i) {
      e.values[i] = cast(e.values[i], slice_type->element_type);
    }
  }

  void visit(SliceIdxExpr &e) override {}
  void visit(Value &val) {}
  void visit(SliceToWireCast &e) override {}
  void visit(TupleToWireCast &e) override {}
  void visit(RegWrite &rw) {}
  void visit(RegRead &rr) {}
  void visit(CreateRegisterExpr &rr) {}
};

void insert_casts(std::shared_ptr<Package> pakage) {
  InsertCastsVisitor v;
  v.visit(*pakage);
}
} // namespace hdlc::ast