#pragma once

#include "hdlc/ast/ast.h"
#include "module.h"
#include <llvm/IR/LLVMContext.h>

#include <memory>

namespace hdlc::jit {
std::unique_ptr<Module>
transform_pkg_to_module(std::unique_ptr<llvm::LLVMContext> ctx,
                        std::shared_ptr<ast::Package> pkg,
                        std::string entrypoint);
}
