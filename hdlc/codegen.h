#pragma once

#include "hdlc/ast.h"
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

#include <memory>
#include <stack>
#include <unordered_map>

struct CodegenVisitor : ast::Visitor {
  llvm::LLVMContext *ctx;
  llvm::IRBuilder<> ir_builder;
  std::stack<llvm::Value *> results_stack;
  llvm::FunctionType *current_function_type;

  std::unordered_map<std::string, llvm::Value *> symbol_table;

  std::unique_ptr<llvm::Module> module;

  void initialize_prebuilt_chips() { add_nand(); }

  void add_nand() {
    auto out =
        llvm::StructType::create({llvm::Type::getInt1Ty(*ctx)}, "NandResult");

    auto sig = llvm::FunctionType::get(
        out, {llvm::Type::getInt1Ty(*ctx), llvm::Type::getInt1Ty(*ctx)}, false);

    auto func = llvm::Function::Create(sig, llvm::Function::ExternalLinkage,
                                       "Nand", module.get());

    auto bb = llvm::BasicBlock::Create(*ctx, "chip_body", func);

    ir_builder.SetInsertPoint(bb);

    auto nand = ir_builder.CreateNot(
        ir_builder.CreateAnd(func->getArg(0), func->getArg(1)));

    llvm::Value *res = ir_builder.CreateAlloca(out);
    res = ir_builder.CreateLoad(out, res);

    ir_builder.CreateInsertValue(res, nand, {0});

    ir_builder.CreateRet(res);
  }

  CodegenVisitor(llvm::LLVMContext *ctx) : ctx(ctx), ir_builder(*ctx) {
    module = std::make_unique<llvm::Module>("mod", *ctx);
    initialize_prebuilt_chips();
  }

  llvm::Type *get_llvm_type(const ast::Type &t) {
    return llvm::Type::getInt1Ty(*ctx);
  }

  virtual void visit(ast::Package &pkg) {
    for (auto &c : pkg.chips) {
      c->visit(*this);
    }
  }
  virtual void visit(ast::Chip &chip) {
    llvm::SmallVector<llvm::Type *, 4> inputs;
    llvm::SmallVector<llvm::Type *, 4> outputs;

    for (auto &i : chip.inputs) {
      inputs.push_back(get_llvm_type(*i->type));
    }
    for (auto &o : chip.outputs) {
      outputs.push_back(get_llvm_type(*o));
    }

    auto out = llvm::StructType::create(outputs);

    auto sig = llvm::FunctionType::get(out, inputs, false);

    auto func = llvm::Function::Create(sig, llvm::Function::ExternalLinkage,
                                       chip.ident, module.get());

    current_function_type = sig;

    auto bb = llvm::BasicBlock::Create(*ctx, "chip_body", func);

    ir_builder.SetInsertPoint(bb);

    symbol_table.clear();

    for (int i = 0; i < chip.inputs.size(); i++) {
      auto arg = func->getArg(i);
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

    auto struct_type = llvm::cast<llvm::StructType>(res->getType());

    for (unsigned i = 0; i < stmt.assignees.size(); i++) {
      auto val =
          ir_builder.CreateExtractValue(res, {i}, stmt.assignees[i]->ident);
      val->setName(stmt.assignees[i]->ident);
      symbol_table[stmt.assignees[i]->ident] = val;
    }
  }

  virtual void visit(ast::CallExpr &expr) {
    auto callee = module->getFunction(expr.chip_name);
    llvm::SmallVector<llvm::Value *, 4> params;

    for (auto &a : expr.args) {
      a->visit(*this);
      params.push_back(results_stack.top());
      results_stack.pop();
    }

    results_stack.push(ir_builder.CreateCall(callee, params));
  }

  virtual void visit(ast::Value &val) {
    results_stack.push(symbol_table[val.ident]);
  }

  virtual void visit(ast::RetStmt &stmt) {
    auto res_type =
        llvm::cast<llvm::StructType>(current_function_type->getReturnType());

    llvm::Value *res_val = ir_builder.CreateAlloca(res_type);
    res_val = ir_builder.CreateLoad(res_type, res_val);

    for (unsigned i = 0; i < stmt.results.size(); i++) {
      stmt.results[i]->visit(*this);
      auto val = results_stack.top();
      results_stack.pop();
      ir_builder.CreateInsertValue(res_val, val, {i});
    }
    ir_builder.CreateRet(res_val);
  }

  virtual void visit(ast::RegInit &reg) {}
  virtual void visit(ast::RegWrite &rw) {}
  virtual void visit(ast::RegRead &rr) {}
};

