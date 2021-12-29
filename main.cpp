#include "hdlc/ast/parser_error.h"
#include "hdlc/chip.h"

#include <fstream>
#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

#include <cassert>
#include <string>
int main() {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  try {
    hdlc::Chip chip("code.txt", "And3");

    assert(chip.get_num_input_slots() == 3);
    assert(chip.get_num_output_slots() == 1);

    auto a = chip.get_input_slot(0);
    auto b = chip.get_input_slot(1);
    auto c = chip.get_input_slot(2);

    auto res = chip.get_output_slot(0);

    a.set(false);
    b.set(false);
    c.set(false);

    chip.run();

    for (int i = 0; i < 2; i++) {
      a.set(i);
      for (int j = 0; j < 2; j++) {
        b.set(j);
        for (int k = 0; k < 2; k++) {
          c.set(k);

          chip.run();

          std::cout << i << ", " << j << ", " << k << " =>" << res.get()
                    << std::endl;
        }
      }
    }
  } catch (hdlc::ast::ParserError &e) {
    std::cout << e.what() << std::endl;
    throw;
  }
}
