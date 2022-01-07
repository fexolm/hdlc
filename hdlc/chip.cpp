#include "chip.h"
#include "hdlc/ast/ast.h"
#include "hdlc/ast/parser.h"
#include "hdlc/jit/codegen.h"
#include "hdlc/jit/module.h"

#include <fstream>
#include <llvm/IR/LLVMContext.h>

namespace hdlc {
Chip::Chip(const std::string &code, const std::string &chip_name) {
  auto pkg = ast::parse_package(code, "gates");

  auto ctx = std::make_unique<llvm::LLVMContext>();

  module = jit::transform_pkg_to_module(std::move(ctx), pkg, chip_name);

  auto requested_chip_iter =
      std::find_if(pkg->chips.begin(), pkg->chips.end(),
                   [&chip_name](auto &c) { return c->ident == chip_name; });

  if (requested_chip_iter == pkg->chips.end()) {
    throw;
  }

  auto requested_chip = *requested_chip_iter;

  reg_buf.resize(module->buffer_size());
}

void Chip::run(int8_t *inputs, int8_t *outputs) {
  module->run(reg_buf.data(), inputs, outputs);
}
} // namespace hdlc
