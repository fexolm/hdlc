#pragma once
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/IR/Module.h>

namespace hdlc::jit {
class Module {
  void (*run_func)(int8_t *, int8_t *, int8_t *);
  std::unique_ptr<llvm::orc::LLJIT> jit;
  llvm::ExitOnError ExitOnErr;
  size_t buf_size;

public:
  Module(std::unique_ptr<llvm::Module> module,
         std::unique_ptr<llvm::LLVMContext> ctx, size_t size);

  void run(int8_t *reg_buf, int8_t *inputs, int8_t *outputs);

  size_t buffer_size();
};
} // namespace hdlc::jit
