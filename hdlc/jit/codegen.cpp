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

public:
  explicit TypeTransformVisitor(llvm::LLVMContext *ctx) : ctx(ctx) {}

  virtual void visit(ast::WireType &wire) {
    results_stack.push(llvm::Type::getInt1Ty(*ctx));
  }

  virtual void visit(ast::RegisterType &reg) { assert(false); }
  virtual void visit(ast::SliceType &slice) { assert(false); }
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

    auto in_ptr = func->getArg(0);
    auto out_ptr = func->getArg(1);

    auto f_res_type = llvm::cast<llvm::PointerType>(f->getArg(0)->getType());
    auto f_res_struct_type =
        llvm::cast<llvm::StructType>(f_res_type->getElementType());
    llvm::SmallVector<llvm::Value *> args;

    auto res = ir_builder.CreateAlloca(f_res_struct_type);
    args.push_back(res);

    for (int i = 0; i < f->arg_size() - 1; i++) {
      llvm::Value *val =
          ir_builder.CreateConstGEP1_32(llvm::Type::getInt8Ty(*ctx), in_ptr, i);

      val = ir_builder.CreateLoad(llvm::Type::getInt8Ty(*ctx), val);
      val = ir_builder.CreateIntCast(val, llvm::Type::getInt1Ty(*ctx), false);

      args.push_back(val);
    }

    ir_builder.CreateCall(f, args);

    for (unsigned i = 0; i < f_res_struct_type->getNumElements(); i++) {
      auto slot = ir_builder.CreateConstGEP1_32(llvm::Type::getInt8Ty(*ctx),
                                                out_ptr, i);

      llvm::Value *val = ir_builder.CreateStructGEP(f_res_struct_type, res, i);
      val = ir_builder.CreateLoad(f_res_struct_type->getElementType(i), val);
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

  llvm::Type *get_llvm_type(ast::Type &t) {
    TypeTransformVisitor v(ctx);
    t.visit(v);
    return v.get_type();
  }

  virtual void visit(ast::Package &pkg) {
    for (auto &c : pkg.chips) {
      c->visit(*this);
    }
    create_run_func();
  }
  virtual void visit(ast::Chip &chip) {
    llvm::SmallVector<llvm::Type *> args;
    llvm::SmallVector<llvm::Type *> outputs;

    for (auto &i : chip.inputs) {
      args.push_back(get_llvm_type(*i->type));
    }

    auto out = get_llvm_type(*chip.output_type);

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
      auto arg = func->getArg(i + 1);
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
      ir_builder.CreateStore(val, slot);
    }
    ir_builder.CreateRetVoid();
  }

  virtual void visit(ast::RegWrite &rw) {}
  virtual void visit(ast::RegRead &rr) {}
};

std::unique_ptr<llvm::Module>
transform_pkg_to_llvm(llvm::LLVMContext *ctx, std::shared_ptr<ast::Package> pkg,
                      std::string entrypoint) {
  CodegenVisitor v(ctx, entrypoint);
  v.visit(*pkg);
  return std::move(v.module);
}
} // namespace hdlc::jit
