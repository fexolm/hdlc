#pragma once
#include "hdlc/ast.h"
#include "hdlc/codegen.h"
#include "hdlc/jit.h"
#include "hdlc/parser.h"

#include <fstream>
#include <llvm/IR/LLVMContext.h>
#include <string>

class Slot {
private:
  bool *ptr;

public:
  explicit Slot(bool *ptr) : ptr(ptr) {}

  Slot(const Slot &) = delete;

  void set(bool val) { *ptr = val; }

  bool get() const { return *ptr; }
};

class Chip {
private:
  std::unique_ptr<JittedModule> jit;

  std::vector<char> inputs;
  std::vector<char> outputs;

public:
  explicit Chip(const std::string &path, const std::string &chip_name) {
    std::ifstream ifs(path);
    std::string code((std::istreambuf_iterator<char>(ifs)),
                     (std::istreambuf_iterator<char>()));

    auto pkg = ast::parse_package(code, "gates");

    auto ctx = std::make_unique<llvm::LLVMContext>();
    CodegenVisitor codegen(ctx.get(), chip_name);
    codegen.visit(*pkg);

    jit = std::make_unique<JittedModule>(std::move(codegen.module),
                                         std::move(ctx));

    auto requested_chip_iter =
        std::find_if(pkg->chips.begin(), pkg->chips.end(),
                     [&chip_name](auto &c) { return c->ident == chip_name; });

    if (requested_chip_iter == pkg->chips.end()) {
      throw;
    }

    auto requested_chip = *requested_chip_iter;

    inputs.resize(requested_chip->inputs.size());
    outputs.resize(requested_chip->outputs.size());
  }

  size_t get_num_input_slots() { return inputs.size(); }

  size_t get_num_output_slots() { return outputs.size(); }

  Slot get_input_slot(size_t i) { return Slot((bool *)&inputs[i]); }

  const Slot get_output_slot(size_t i) { return Slot((bool *)&outputs[i]); }

  void run() { jit->run((bool *)inputs.data(), (bool *)outputs.data()); }
};
