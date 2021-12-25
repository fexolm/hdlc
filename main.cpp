#include "hdlc/codegen.h"
#include "hdlc/parser.h"
#include <fstream>
#include <hdlc/codegen.h>
#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>

#include <string>

int main() {
  std::ifstream ifs("code.txt");
  std::string code((std::istreambuf_iterator<char>(ifs)),
                   (std::istreambuf_iterator<char>()));

  Parser p(code);
  try {
    auto pkg = p.read_package("gates");
    ast::Printer v(std::cout);
    v.visit(*pkg);

    llvm::LLVMContext ctx;

    CodegenVisitor codegen(&ctx);

    codegen.visit(*pkg);

    auto module = std::move(codegen.module);

    module->print(llvm::outs(), nullptr);

  } catch (ParserError &e) {
    std::cout << e.what() << std::endl;
    throw;
  }
}
