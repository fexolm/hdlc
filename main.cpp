#include "hdlc/codegen.h"
#include "hdlc/jit.h"
#include "hdlc/parser.h"
#include <fstream>
#include <hdlc/codegen.h>
#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include <string>
int main() {
  std::ifstream ifs("code.txt");
  std::string code((std::istreambuf_iterator<char>(ifs)),
                   (std::istreambuf_iterator<char>()));
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  Parser p(code);
  try {
    auto pkg = p.read_package("gates");
    ast::Printer v(std::cout);
    v.visit(*pkg);

    auto ctx = std::make_unique<llvm::LLVMContext>();

    CodegenVisitor codegen(ctx.get(), "And3");

    codegen.visit(*pkg);

    auto module = std::move(codegen.module);

    module->print(llvm::outs(), nullptr);

    if (llvm::verifyModule(*module, &llvm::outs())) {
      throw;
    }

    JittedModule jmod(std::move(module), std::move(ctx));

    {
      std::vector<char> inputs{true, true, true};
      std::vector<char> outputs{true};
      jmod.run((bool *)inputs.data(), (bool *)outputs.data());
      std::cout << "Result: " << (bool)outputs[0] << std::endl;
    }

    {
      std::vector<char> inputs{true, false, true};
      std::vector<char> outputs{false};
      jmod.run((bool *)inputs.data(), (bool *)outputs.data());
      std::cout << "Result: " << (bool)outputs[0] << std::endl;
    }

    {
      std::vector<char> inputs{true, true, false};
      std::vector<char> outputs{false};
      jmod.run((bool *)inputs.data(), (bool *)outputs.data());
      std::cout << "Result: " << (bool)outputs[0] << std::endl;
    }

    {
      std::vector<char> inputs{false, true, true};
      std::vector<char> outputs{false};
      jmod.run((bool *)inputs.data(), (bool *)outputs.data());
      std::cout << "Result: " << (bool)outputs[0] << std::endl;
    }

  } catch (ParserError &e) {
    std::cout << e.what() << std::endl;
    throw;
  }
}
