#include "chip.h"
#include "hdlc/ast/ast.h"
#include "hdlc/ast/parser.h"
#include "hdlc/jit/codegen.h"
#include "hdlc/jit/module.h"

#include <fstream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/TargetSelect.h>

#include <mutex>

namespace hdlc {

struct ChipImpl : Chip {
  std::unique_ptr<jit::Module> module;
  std::vector<int8_t> reg_buf;

  ChipImpl(const std::string &code, const std::string &chip_name)
      : module(nullptr), reg_buf(0) {
    static std::once_flag llvm_initialized;

    std::call_once(llvm_initialized, []() {
      llvm::InitializeNativeTarget();
      llvm::InitializeNativeTargetAsmPrinter();
    });

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

  void run(int8_t *inputs, int8_t *outputs) override {
    module->run(reg_buf.data(), inputs, outputs);
  }
};

std::shared_ptr<Chip> create_chip(const std::string &code,
                                  const std::string &chip_name) {
  return std::make_shared<ChipImpl>(code, chip_name);
}
} // namespace hdlc
