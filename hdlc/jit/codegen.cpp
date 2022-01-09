#include "codegen.h"

#include "hdlc/ast/ast.h"
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <memory>
#include <stack>
#include <unordered_map>
namespace hdlc::jit {

struct RegMemCounter : ast::Visitor {
  std::unordered_map<std::string, size_t> mem_per_chip;
  size_t current_size = 0;

  void visit(ast::Package &pkg) override {
    for (auto &c : pkg.chips) {
      c->visit(*this);
    }
  }

  void visit(ast::Chip &chip) override {
    current_size = 0;
    for (auto &s : chip.body) {
      s->visit(*this);
    }
    mem_per_chip[chip.ident] = current_size;
  }

  void visit(ast::AssignStmt &stmt) override { stmt.rhs->visit(*this); }

  void visit(ast::CallExpr &expr) override {
    current_size += mem_per_chip[expr.chip_name];
  }

  void visit(ast::Value &) override {}

  void visit(ast::RetStmt &stmt) override {
    for (auto &e : stmt.results) {
      e->visit(*this);
    }
  }

  void visit(ast::RegWrite &rw) override { rw.rhs->visit(*this); }

  void visit(ast::RegRead &) override {}

  void visit(ast::SliceIdxExpr &) override {}

  void visit(ast::SliceJoinExpr &e) override {
    for (auto v : e.values) {
      v->visit(*this);
    }
  }

  void visit(ast::SliceToWireCast &e) override { e.expr->visit(*this); }

  void visit(ast::TupleToWireCast &e) override { e.expr->visit(*this); }

  void visit(ast::CreateRegisterExpr &e) override {
    if (auto t = std::dynamic_pointer_cast<ast::SliceType>(e.result_type())) {
      current_size += t->size;
    } else {
      current_size++;
    }
  }
};

struct TypeTransformVisitor : ast::TypeVisitor {
private:
  std::stack<llvm::Type *> results_stack;
  llvm::LLVMContext *ctx;
  bool array_as_ptr;

public:
  explicit TypeTransformVisitor(llvm::LLVMContext *ctx, bool array_as_ptr)
      : ctx(ctx), array_as_ptr(array_as_ptr) {}

  void visit(ast::WireType &) override {
    results_stack.push(llvm::Type::getInt8Ty(*ctx));
  }

  void visit(ast::RegisterType &) override {
    results_stack.push(llvm::Type::getInt8Ty(*ctx));
  }

  void visit(ast::SliceType &slice) override {
    slice.element_type->visit(*this);
    auto element_type = results_stack.top();
    results_stack.pop();
    if (array_as_ptr) {
      results_stack.push(element_type->getPointerTo());
    } else {
      results_stack.push(llvm::ArrayType::get(element_type, slice.size));
    }
  }

  void visit(ast::TupleType &tuple) override {
    llvm::SmallVector<llvm::Type *> field_types;

    for (auto &t : tuple.element_types) {
      t->visit(*this);
      field_types.push_back(results_stack.top());
      results_stack.pop();
    }

    results_stack.push(llvm::StructType::create(field_types));
  }

  llvm::Type *get_type() {
    auto res = results_stack.top();
    results_stack.pop();
    return res;
  }
};

struct CodegenVisitor : ast::Visitor {
  llvm::LLVMContext *ctx;
  llvm::IRBuilder<> ir_builder;
  std::stack<llvm::Value *> results_stack;
  llvm::Function *current_function;

  std::string entrypoint;

  std::unordered_map<std::string, llvm::Value *> symbol_table;
  std::unordered_map<std::string, ast::Chip *> chips;
  std::unordered_map<std::string, size_t> mem_per_chip;

  std::unique_ptr<llvm::Module> module;

  llvm::BasicBlock *update_reg_block;

  size_t reg_buf_offset = 0;

  void initialize_prebuilt_chips() { add_nand(); }

  void create_run_func() {
    auto out = llvm::Type::getVoidTy(*ctx);
    auto buf_ty = llvm::Type::getInt8Ty(*ctx)->getPointerTo();

    auto sig = llvm::FunctionType::get(out, {buf_ty, buf_ty, buf_ty}, false);

    auto func = llvm::Function::Create(sig, llvm::Function::ExternalLinkage,
                                       "run", module.get());

    auto bb = llvm::BasicBlock::Create(*ctx, "run_body", func);

    ir_builder.SetInsertPoint(bb);

    auto f = module->getFunction(entrypoint);
    auto chip = chips[entrypoint];

    auto reg_buf = func->getArg(0);
    auto in_ptr = func->getArg(1);
    auto out_ptr = func->getArg(2);

    auto f_res_type = llvm::cast<llvm::PointerType>(f->getArg(0)->getType());
    auto f_res_struct_type =
        llvm::cast<llvm::StructType>(f_res_type->getElementType());

    llvm::SmallVector<llvm::Value *> args;

    auto res = ir_builder.CreateAlloca(f_res_struct_type);
    args.push_back(res);

    args.push_back(reg_buf);

    size_t offset = 0;

    for (size_t arg_num = 0; arg_num < f->arg_size() - 2; arg_num++) {
      auto type = chip->inputs[arg_num]->result_type();
      if (auto st = std::dynamic_pointer_cast<ast::SliceType>(type)) {
        auto arr = ir_builder.CreateAlloca(ir_builder.getInt8Ty(),
                                           ir_builder.getInt32(st->size));

        for (size_t i = 0; i < st->size; i++) {
          auto slot =
              ir_builder.CreateConstGEP1_32(ir_builder.getInt8Ty(), arr, i);

          llvm::Value *val = ir_builder.CreateConstGEP1_32(
              llvm::Type::getInt8Ty(*ctx), in_ptr, offset + i);

          val = ir_builder.CreateLoad(llvm::Type::getInt8Ty(*ctx), val);

          ir_builder.CreateStore(val, slot);
        }

        args.push_back(arr);

        offset += st->size;
        continue;
      }

      assert(std::dynamic_pointer_cast<ast::WireType>(type));

      llvm::Value *val = ir_builder.CreateConstGEP1_32(
          llvm::Type::getInt8Ty(*ctx), in_ptr, offset);

      val = ir_builder.CreateLoad(llvm::Type::getInt8Ty(*ctx), val);
      val = ir_builder.CreateIntCast(val, llvm::Type::getInt8Ty(*ctx), false);

      args.push_back(val);
      offset++;
    }

    ir_builder.CreateCall(f, args);

    offset = 0;

    for (unsigned res_num = 0; res_num < f_res_struct_type->getNumElements();
         res_num++) {
      auto type = chip->output_type->element_types[res_num];

      llvm::Value *val =
          ir_builder.CreateStructGEP(f_res_struct_type, res, res_num);

      if (auto st = std::dynamic_pointer_cast<ast::SliceType>(type)) {

        for (size_t i = 0; i < st->size; i++) {
          auto slot = ir_builder.CreateConstGEP1_32(llvm::Type::getInt8Ty(*ctx),
                                                    out_ptr, i + offset);

          auto val_i = ir_builder.CreateConstGEP2_32(
              f_res_struct_type->getElementType(res_num), val, 0, i);

          val_i = ir_builder.CreateLoad(ir_builder.getInt8Ty(), val_i);
          val_i = ir_builder.CreateIntCast(val_i, llvm::Type::getInt8Ty(*ctx),
                                           false);

          ir_builder.CreateStore(val_i, slot);
        }

        offset += st->size;
        continue;
      }

      assert(std::dynamic_pointer_cast<ast::WireType>(type));

      auto slot = ir_builder.CreateConstGEP1_32(llvm::Type::getInt8Ty(*ctx),
                                                out_ptr, offset);

      val = ir_builder.CreateLoad(llvm::Type::getInt8Ty(*ctx), val);
      val = ir_builder.CreateIntCast(val, llvm::Type::getInt8Ty(*ctx), false);

      ir_builder.CreateStore(val, slot);
    }

    ir_builder.CreateRetVoid();
  }

  void add_nand() {
    auto out =
        llvm::StructType::create({llvm::Type::getInt8Ty(*ctx)}, "NandResult");

    auto sig = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*ctx),
        {out->getPointerTo(), llvm::Type::getInt8Ty(*ctx)->getPointerTo(),
         llvm::Type::getInt8Ty(*ctx), llvm::Type::getInt8Ty(*ctx)},
        false);

    auto func = llvm::Function::Create(sig, llvm::Function::PrivateLinkage,
                                       "Nand", module.get());

    auto bb = llvm::BasicBlock::Create(*ctx, "chip_body", func);

    ir_builder.SetInsertPoint(bb);

    auto res_ptr = func->getArg(0);
    auto nand = ir_builder.CreateNot(
        ir_builder.CreateAnd(func->getArg(2), func->getArg(3)));

    auto res_slot = ir_builder.CreateStructGEP(out, res_ptr, 0);

    ir_builder.CreateStore(nand, res_slot);

    ir_builder.CreateRetVoid();
  }

  CodegenVisitor(llvm::LLVMContext *ctx, std::string entrypoint)
      : ctx(ctx), ir_builder(*ctx), entrypoint(entrypoint) {
    module = std::make_unique<llvm::Module>("mod", *ctx);
    initialize_prebuilt_chips();
  }

  llvm::Type *get_llvm_type(std::shared_ptr<ast::Type> t,
                            bool array_as_ptr = false) {
    TypeTransformVisitor v(ctx, array_as_ptr);
    t->visit(v);
    return v.get_type();
  }

  void visit(ast::Package &pkg) override {
    RegMemCounter c;
    c.visit(pkg);
    mem_per_chip = std::move(c.mem_per_chip);
    reg_buf_offset = 0;

    for (auto &c : pkg.chips) {
      c->visit(*this);
    }
    create_run_func();
  }

  void visit(ast::Chip &chip) override {
    reg_buf_offset = 0;
    if (chip.ident == "Nand") {
      return;
    }

    chips[chip.ident] = &chip;

    llvm::SmallVector<llvm::Type *> args;
    llvm::SmallVector<llvm::Type *> outputs;

    auto out = get_llvm_type(chip.output_type);
    args.push_back(out->getPointerTo());
    args.push_back(ir_builder.getInt8Ty()->getPointerTo());

    for (auto &i : chip.inputs) {
      args.push_back(get_llvm_type(i->type, true));
    }

    auto sig =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx), args, false);

    auto func = llvm::Function::Create(sig, llvm::Function::PrivateLinkage,
                                       chip.ident, module.get());

    current_function = func;

    auto bb = llvm::BasicBlock::Create(*ctx, "chip_body", func);

    update_reg_block = llvm::BasicBlock::Create(*ctx, "update_regs", func);

    ir_builder.SetInsertPoint(bb);

    symbol_table.clear();

    for (size_t i = 0; i < chip.inputs.size(); i++) {
      llvm::Value *arg = func->getArg(i + 2);
      arg->setName(chip.inputs[i]->ident);
      symbol_table[chip.inputs[i]->ident] = arg;
    }

    for (auto &s : chip.body) {
      s->visit(*this);
    }
  }

  void visit(ast::AssignStmt &stmt) override {
    stmt.rhs->visit(*this);

    auto res = results_stack.top();
    results_stack.pop();

    if (std::dynamic_pointer_cast<ast::RegisterType>(stmt.rhs->result_type())) {
      symbol_table[stmt.assignees[0]->ident] = res;
      return;
    }

    if (std::dynamic_pointer_cast<ast::WireType>(stmt.rhs->result_type())) {
      symbol_table[stmt.assignees[0]->ident] = res;
      return;
    }

    if (std::dynamic_pointer_cast<ast::SliceType>(stmt.rhs->result_type())) {
      symbol_table[stmt.assignees[0]->ident] = res;
      return;
    }

    auto struct_ptr_type = llvm::cast<llvm::PointerType>(res->getType());
    auto struct_type =
        llvm::cast<llvm::StructType>(struct_ptr_type->getElementType());

    auto tuple_type =
        std::static_pointer_cast<ast::TupleType>(stmt.rhs->result_type());

    for (unsigned i = 0; i < stmt.assignees.size(); i++) {
      auto type = tuple_type->element_types[i];

      auto val_ptr = ir_builder.CreateStructGEP(struct_type, res, i,
                                                stmt.assignees[i]->ident);

      llvm::Value *val{nullptr};

      if (auto st = std::dynamic_pointer_cast<ast::SliceType>(type)) {
        val = ir_builder.CreateConstGEP2_32(struct_type->getElementType(i),
                                            val_ptr, 0, i);
      } else {
        val = ir_builder.CreateLoad(struct_type->getElementType(i), val_ptr);
      }

      val->setName(stmt.assignees[i]->ident);
      symbol_table[stmt.assignees[i]->ident] = val;
    }
  }

  void visit(ast::CallExpr &expr) override {
    auto callee = module->getFunction(expr.chip_name);
    llvm::SmallVector<llvm::Value *> params;

    auto res_type = llvm::cast<llvm::PointerType>(callee->getArg(0)->getType());
    auto res_struct = llvm::cast<llvm::StructType>(res_type->getElementType());

    auto res = ir_builder.CreateAlloca(res_struct);
    auto reg_buf = ir_builder.CreateConstGEP1_32(
        ir_builder.getInt8Ty(), current_function->getArg(1), reg_buf_offset);

    reg_buf_offset += mem_per_chip[expr.chip_name];

    params.push_back(res);
    params.push_back(reg_buf);

    for (auto &a : expr.args) {
      a->visit(*this);
      params.push_back(results_stack.top());
      results_stack.pop();
    }

    ir_builder.CreateCall(callee, params);
    results_stack.push(res);
  }

  void visit(ast::Value &val) override {
    results_stack.push(symbol_table[val.ident]);
  }

  void store(llvm::Value *slot_ptr, llvm::Value *val,
             std::shared_ptr<ast::Type> type) {
    if (auto t = std::dynamic_pointer_cast<ast::SliceType>(type)) {
      for (size_t i = 0; i < t->size; ++i) {
        auto pointee_type = get_llvm_type(t->element_type);
        auto p = llvm::cast<llvm::PointerType>(slot_ptr->getType());
        auto res_slot =
            ir_builder.CreateConstGEP2_32(p->getElementType(), slot_ptr, 0, i);

        auto p2 = llvm::cast<llvm::PointerType>(val->getType());
        auto val_slot =
            ir_builder.CreateConstGEP1_32(p2->getElementType(), val, i);
        auto val_i = ir_builder.CreateLoad(pointee_type, val_slot);
        ir_builder.CreateStore(val_i, res_slot);
      }

      return;
    }

    ir_builder.CreateStore(val, slot_ptr);
  }

  void visit(ast::RetStmt &stmt) override {
    auto res_ptr = current_function->getArg(0);
    auto res_type = llvm::cast<llvm::PointerType>(res_ptr->getType());
    auto res_struct_type =
        llvm::cast<llvm::StructType>(res_type->getElementType());

    for (unsigned i = 0; i < stmt.results.size(); i++) {
      auto slot = ir_builder.CreateStructGEP(res_struct_type, res_ptr, i);
      stmt.results[i]->visit(*this);
      auto val = results_stack.top();
      results_stack.pop();

      store(slot, val, stmt.results[i]->result_type());
    }
    ir_builder.CreateBr(update_reg_block);
    ir_builder.SetInsertPoint(update_reg_block);
    ir_builder.CreateRetVoid();
  }

  void visit(ast::SliceJoinExpr &expr) override {
    auto res_type =
        llvm::cast<llvm::PointerType>(get_llvm_type(expr.result_type(), true));

    llvm::Value *slice = ir_builder.CreateAlloca(
        res_type->getElementType(), ir_builder.getInt64(expr.values.size()));

    for (size_t i = 0; i < expr.values.size(); ++i) {
      auto slot =
          ir_builder.CreateConstGEP1_32(res_type->getElementType(), slice, i);
      expr.values[i]->visit(*this);
      auto val = results_stack.top();
      results_stack.pop();
      ir_builder.CreateStore(val, slot);
    }

    results_stack.push(slice);
  }

  void visit(ast::SliceIdxExpr &expr) override {
    expr.slice->visit(*this);
    auto slice = results_stack.top();
    results_stack.pop();

    auto ptr_type = llvm::cast<llvm::PointerType>(
        get_llvm_type(expr.slice->result_type(), true));

    auto begin_ptr = ir_builder.CreateConstGEP1_32(ptr_type->getElementType(),
                                                   slice, expr.begin);

    results_stack.push(begin_ptr);
  }

  void visit(ast::SliceToWireCast &e) override {
    e.expr->visit(*this);
    auto slice = results_stack.top();
    results_stack.pop();

    results_stack.push(
        ir_builder.CreateLoad(get_llvm_type(e.result_type()), slice));
  }
  void visit(ast::TupleToWireCast &e) override {
    e.expr->visit(*this);
    auto struct_ptr = results_stack.top();
    results_stack.pop();

    auto ptr_type = llvm::cast<llvm::PointerType>(struct_ptr->getType());

    auto element_ptr =
        ir_builder.CreateStructGEP(ptr_type->getElementType(), struct_ptr, 0);

    results_stack.push(
        ir_builder.CreateLoad(get_llvm_type(e.result_type()), element_ptr));
  }

  void visit(ast::CreateRegisterExpr &e) override {
    auto buf = ir_builder.CreateConstGEP1_32(
        ir_builder.getInt8Ty(), current_function->getArg(1), reg_buf_offset);
    if (auto t = std::dynamic_pointer_cast<ast::SliceType>(e.result_type())) {
      reg_buf_offset += t->size;
    } else {
      reg_buf_offset++;
    }
    results_stack.push(buf);
  }

  void visit(ast::RegWrite &rw) override {
    rw.reg->visit(*this);
    auto reg = results_stack.top();
    results_stack.pop();
    rw.rhs->visit(*this);
    auto val = results_stack.top();
    results_stack.pop();

    auto ip = ir_builder.saveIP();
    ir_builder.SetInsertPoint(update_reg_block);

    if (auto st =
            std::dynamic_pointer_cast<ast::SliceType>(rw.reg->result_type())) {
      for (size_t i = 0; i < st->size; ++i) {
        auto slot =
            ir_builder.CreateConstGEP1_32(ir_builder.getInt8Ty(), reg, i);
        auto val_slot =
            ir_builder.CreateConstGEP1_32(ir_builder.getInt8Ty(), val, i);
        auto val_i = ir_builder.CreateLoad(ir_builder.getInt8Ty(), val_slot);
        ir_builder.CreateStore(val_i, slot);
      }
    } else {
      ir_builder.CreateStore(val, reg);
    }

    ir_builder.restoreIP(ip);
  }

  void visit(ast::RegRead &rr) override {
    rr.reg->visit(*this);
    auto val_ptr = results_stack.top();
    results_stack.pop();
    if (std::dynamic_pointer_cast<ast::SliceType>(rr.result_type())) {
      results_stack.push(val_ptr);
    } else {
      results_stack.push(
          ir_builder.CreateLoad(get_llvm_type(rr.result_type()), val_ptr));
    }
  }
};

std::unique_ptr<Module>
transform_pkg_to_module(std::unique_ptr<llvm::LLVMContext> ctx,
                        std::shared_ptr<ast::Package> pkg,
                        std::string entrypoint) {

  CodegenVisitor v(ctx.get(), entrypoint);
  v.visit(*pkg);
  auto size = v.mem_per_chip[entrypoint];

  return std::make_unique<Module>(std::move(v.module), std::move(ctx), size);
}
} // namespace hdlc::jit
