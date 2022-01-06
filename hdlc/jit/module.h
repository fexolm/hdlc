#pragma once
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>

namespace hdlc::jit {
class Module {
  void (*run_func)(int8_t *, int8_t *);
  std::unique_ptr<llvm::orc::LLJIT> jit;
  llvm::ExitOnError ExitOnErr;

public:
  Module(std::unique_ptr<llvm::Module> module,
         std::unique_ptr<llvm::LLVMContext> ctx);

  void run(int8_t *inputs, int8_t *outputs);
};
} // namespace hdlc::jit
