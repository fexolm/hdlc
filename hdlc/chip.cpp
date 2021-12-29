#include "chip.h"
#include "hdlc/ast/ast.h"
#include "hdlc/ast/parser.h"
#include "hdlc/jit/codegen.h"
#include "hdlc/jit/module.h"

#include <fstream>
#include <llvm/IR/LLVMContext.h>

namespace hdlc {
Slot::Slot(bool *ptr) : ptr(ptr) {}

void Slot::set(bool val) { *ptr = val; }

bool Slot::get() const { return *ptr; }

Chip::Chip(const std::string &path, const std::string &chip_name) {
  std::ifstream ifs(path);
  std::string code((std::istreambuf_iterator<char>(ifs)),
                   (std::istreambuf_iterator<char>()));

  auto pkg = ast::parse_package(code, "gates");

  auto ctx = std::make_unique<llvm::LLVMContext>();

  module = std::make_unique<jit::Module>(
      jit::transform_pkg_to_llvm(ctx.get(), pkg, chip_name), std::move(ctx));

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

size_t Chip::get_num_input_slots() { return inputs.size(); }

size_t Chip::get_num_output_slots() { return outputs.size(); }

Slot Chip::get_input_slot(size_t i) { return Slot((bool *)&inputs[i]); }

const Slot Chip::get_output_slot(size_t i) { return Slot((bool *)&outputs[i]); }

void Chip::run() { module->run((bool *)inputs.data(), (bool *)outputs.data()); }
} // namespace hdlc
