#pragma once

#include "hdlc/ast/ast.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include <memory>

namespace hdlc::jit {
std::unique_ptr<llvm::Module>
transform_pkg_to_llvm(llvm::LLVMContext *ctx, std::shared_ptr<ast::Package> pkg,
                      std::string entrypoint);
}
