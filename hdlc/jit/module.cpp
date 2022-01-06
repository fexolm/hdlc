#include "module.h"

namespace hdlc::jit {

Module::Module(std::unique_ptr<llvm::Module> module,
               std::unique_ptr<llvm::LLVMContext> ctx) {

  llvm::orc::LLJITBuilder jit_builder;
  jit = ExitOnErr(jit_builder.create());

  llvm::orc::ThreadSafeModule m(std::move(module), std::move(ctx));
  ExitOnErr(jit->addIRModule(std::move(m)));

  auto f = ExitOnErr(jit->lookup("run"));

  run_func = (decltype(run_func))f.getAddress();
}

void Module::run(int8_t *inputs, int8_t *outputs) { run_func(inputs, outputs); }
} // namespace hdlc::jit