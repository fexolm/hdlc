#pragma once
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>

class JittedModule {
  void (*run_func)(bool *, bool *);
  std::unique_ptr<llvm::orc::LLJIT> jit;
  llvm::ExitOnError ExitOnErr;

public:
  JittedModule(std::unique_ptr<llvm::Module> module,
               std::unique_ptr<llvm::LLVMContext> ctx) {

    llvm::orc::LLJITBuilder jit_builder;
    jit = ExitOnErr(jit_builder.create());

    llvm::orc::ThreadSafeModule m(std::move(module), std::move(ctx));
    ExitOnErr(jit->addIRModule(std::move(m)));

    auto f = ExitOnErr(jit->lookup("run"));

    run_func = (decltype(run_func))f.getAddress();
  }

  void run(bool *inputs, bool *outputs) { run_func(inputs, outputs); }
};
