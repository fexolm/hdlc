#include "module.h"

namespace hdlc::jit {

Module::Module(std::unique_ptr<llvm::Module> module,
               std::unique_ptr<llvm::LLVMContext> ctx, size_t buf_size)
    : buf_size(buf_size) {

  llvm::orc::LLJITBuilder jit_builder;
  jit = ExitOnErr(jit_builder.create());

  llvm::orc::ThreadSafeModule m(std::move(module), std::move(ctx));
  ExitOnErr(jit->addIRModule(std::move(m)));

  auto f = ExitOnErr(jit->lookup("run"));

  run_func = (decltype(run_func))f.getAddress();
}

void Module::run(int8_t *reg_buf, int8_t *inputs, int8_t *outputs) {
  run_func(reg_buf, inputs, outputs);
}

size_t Module::buffer_size() { return buf_size; }

} // namespace hdlc::jit