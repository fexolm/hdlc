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

struct TypeTransformVisitor : ast::TypeVisitor {
private:
  std::stack<llvm::Type *> results_stack;
  llvm::LLVMContext *ctx;
  bool array_as_ptr;

public:
  explicit TypeTransformVisitor(llvm::LLVMContext *ctx, bool array_as_ptr)
      : ctx(ctx), array_as_ptr(array_as_ptr) {}

  virtual void visit(ast::WireType &wire) {
    results_stack.push(llvm::Type::getInt1Ty(*ctx));
  }

  virtual void visit(ast::RegisterType &reg) { assert(false); }

  virtual void visit(ast::SliceType &slice) {
    slice.element_type->visit(*this);
    auto element_type = results_stack.top();
    results_stack.pop();
    if (array_as_ptr) {
      results_stack.push(element_type->getPointerTo());
    } else {
      results_stack.push(llvm::ArrayType::get(element_type, slice.size));
    }
  }

  virtual void visit(ast::TupleType &tuple) {
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

  std::unique_ptr<llvm::Module> module;

  void initialize_prebuilt_chips() { add_nand(); }

  void create_run_func() {
    auto out = llvm::Type::getVoidTy(*ctx);
    auto bool_array = llvm::Type::getInt8Ty(*ctx)->getPointerTo();

    auto sig = llvm::FunctionType::get(out, {bool_array, bool_array}, false);

    auto func = llvm::Function::Create(sig, llvm::Function::ExternalLinkage,
                                       "run", module.get());

    auto bb = llvm::BasicBlock::Create(*ctx, "run_body", func);

    ir_builder.SetInsertPoint(bb);

    auto f = module->getFunction(entrypoint);
    auto chip = chips[entrypoint];

    auto in_ptr = func->getArg(0);
    auto out_ptr = func->getArg(1);

    auto f_res_type = llvm::cast<llvm::PointerType>(f->getArg(0)->getType());
    auto f_res_struct_type =
        llvm::cast<llvm::StructType>(f_res_type->getElementType());

    llvm::SmallVector<llvm::Value *> args;

    auto res = ir_builder.CreateAlloca(f_res_struct_type);
    args.push_back(res);

    size_t offset = 0;

    for (int arg_num = 0; arg_num < f->arg_size() - 1; arg_num++) {
      auto type = chip->inputs[arg_num]->result_type();
      if (auto st = std::dynamic_pointer_cast<ast::SliceType>(type)) {
        auto arr = ir_builder.CreateAlloca(ir_builder.getInt1Ty(),
                                           ir_builder.getInt32(st->size));

        for (int i = 0; i < st->size; i++) {
          auto slot =
              ir_builder.CreateConstGEP1_32(ir_builder.getInt1Ty(), arr, i);

          llvm::Value *val = ir_builder.CreateConstGEP1_32(
              llvm::Type::getInt8Ty(*ctx), in_ptr, offset + i);

          val = ir_builder.CreateLoad(llvm::Type::getInt8Ty(*ctx), val);
          val =
              ir_builder.CreateIntCast(val, llvm::Type::getInt1Ty(*ctx), false);

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
      val = ir_builder.CreateIntCast(val, llvm::Type::getInt1Ty(*ctx), false);

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

        for (int i = 0; i < st->size; i++) {
          auto slot = ir_builder.CreateConstGEP1_32(llvm::Type::getInt8Ty(*ctx),
                                                    out_ptr, i + offset);

          auto val_i = ir_builder.CreateConstGEP2_32(
              f_res_struct_type->getElementType(res_num), val, 0, i);

          val_i = ir_builder.CreateLoad(ir_builder.getInt1Ty(), val_i);
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

      val = ir_builder.CreateLoad(llvm::Type::getInt1Ty(*ctx), val);
      val = ir_builder.CreateIntCast(val, llvm::Type::getInt8Ty(*ctx), false);

      ir_builder.CreateStore(val, slot);
    }

    ir_builder.CreateRetVoid();
  }

  void add_nand() {
    auto out =
        llvm::StructType::create({llvm::Type::getInt1Ty(*ctx)}, "NandResult");

    auto sig = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx),
                                       {out->getPointerTo(),
                                        llvm::Type::getInt1Ty(*ctx),
                                        llvm::Type::getInt1Ty(*ctx)},
                                       false);

    auto func = llvm::Function::Create(sig, llvm::Function::PrivateLinkage,
                                       "Nand", module.get());

    auto bb = llvm::BasicBlock::Create(*ctx, "chip_body", func);

    ir_builder.SetInsertPoint(bb);

    auto res_ptr = func->getArg(0);
    auto nand = ir_builder.CreateNot(
        ir_builder.CreateAnd(func->getArg(1), func->getArg(2)));

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

  virtual void visit(ast::Package &pkg) {
    for (auto &c : pkg.chips) {
      c->visit(*this);
    }
    create_run_func();
  }

  virtual void visit(ast::Chip &chip) {
    if (chip.ident == "Nand") {
      return;
    }

    chips[chip.ident] = &chip;

    llvm::SmallVector<llvm::Type *> args;
    llvm::SmallVector<llvm::Type *> outputs;

    for (auto &i : chip.inputs) {
      args.push_back(get_llvm_type(i->type, true));
    }

    auto out = get_llvm_type(chip.output_type);

    args.insert(args.begin(), out->getPointerTo());

    auto sig =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx), args, false);

    auto func = llvm::Function::Create(sig, llvm::Function::PrivateLinkage,
                                       chip.ident, module.get());

    current_function = func;

    auto bb = llvm::BasicBlock::Create(*ctx, "chip_body", func);

    ir_builder.SetInsertPoint(bb);

    symbol_table.clear();

    for (int i = 0; i < chip.inputs.size(); i++) {
      llvm::Value *arg = func->getArg(i + 1);
      arg->setName(chip.inputs[i]->ident);
      symbol_table[chip.inputs[i]->ident] = arg;
    }

    for (auto &s : chip.body) {
      s->visit(*this);
    }
  }

  virtual void visit(ast::AssignStmt &stmt) {
    stmt.rhs->visit(*this);

    auto res = results_stack.top();
    results_stack.pop();

    auto struct_ptr_type = llvm::cast<llvm::PointerType>(res->getType());
    auto struct_type =
        llvm::cast<llvm::StructType>(struct_ptr_type->getElementType());

    for (unsigned i = 0; i < stmt.assignees.size(); i++) {
      auto val_ptr = ir_builder.CreateStructGEP(struct_type, res, i,
                                                stmt.assignees[i]->ident);
      auto val = ir_builder.CreateLoad(struct_type->getElementType(i), val_ptr);
      val->setName(stmt.assignees[i]->ident);
      symbol_table[stmt.assignees[i]->ident] = val;
    }
  }

  virtual void visit(ast::CallExpr &expr) {
    auto callee = module->getFunction(expr.chip_name);
    llvm::SmallVector<llvm::Value *> params;

    auto res_type = llvm::cast<llvm::PointerType>(callee->getArg(0)->getType());
    auto res_struct = llvm::cast<llvm::StructType>(res_type->getElementType());

    auto res = ir_builder.CreateAlloca(res_struct);

    params.push_back(res);

    for (auto &a : expr.args) {
      a->visit(*this);
      params.push_back(results_stack.top());
      results_stack.pop();
    }

    ir_builder.CreateCall(callee, params);
    results_stack.push(res);
  }

  virtual void visit(ast::Value &val) {
    results_stack.push(symbol_table[val.ident]);
  }

  void store(llvm::Value *slot_ptr, llvm::Value *val,
             std::shared_ptr<ast::Type> type) {
    if (auto t = std::dynamic_pointer_cast<ast::SliceType>(type)) {
      for (int i = 0; i < t->size; ++i) {
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

  virtual void visit(ast::RetStmt &stmt) {
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
    ir_builder.CreateRetVoid();
  }

  virtual void visit(ast::RegWrite &rw) {}
  virtual void visit(ast::RegRead &rr) {}

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
};

std::unique_ptr<llvm::Module>
transform_pkg_to_llvm(llvm::LLVMContext *ctx, std::shared_ptr<ast::Package> pkg,
                      std::string entrypoint) {
  CodegenVisitor v(ctx, entrypoint);
  v.visit(*pkg);
  return std::move(v.module);
}
} // namespace hdlc::jit
