#include "gtest_util.h"
#include "hdlc/ast/parser.h"
#include "hdlc/ast/parser_error.h"
#include "hdlc/chip.h"
#include "gtest/gtest.h"
#include <llvm/Support/TargetSelect.h>

std::string g_code = R"(
chip And (a, b) res {
    tmp := Nand(a, b)
    res := Nand(tmp, tmp)
    return res
}

chip And3(a, b, c) res {
	tmp := And(a, b)
	res := And(tmp, c)
	return res
}

chip And4Way(a[4], b[4]) res[4] {
  return [
    And(a[0], b[0]), 
    And(a[1], b[1]), 
    And(a[2], b[2]), 
    And(a[3], b[3])
  ]
}
)";

class TestChips : public ::testing::Test {
protected:
  void SetUp() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
  }

  void compare_results(hdlc::Chip &chip, std::vector<char> inputs,
                       std::vector<char> expected_outputs) {
    for (int i = 0; i < chip.get_num_input_slots(); ++i) {
      auto slot = chip.get_input_slot(i);
      slot.set(inputs[i]);
    }
    chip.run();

    for (int i = 0; i < chip.get_num_output_slots(); ++i) {
      auto slot = chip.get_output_slot(i);
      EXPECT_EQ(slot.get(), expected_outputs[i]);
    }
  }
};

TEST_F(TestChips, And) {
  hdlc::Chip chip(g_code, "And");
  compare_results(chip, {0, 0}, {0});
  compare_results(chip, {0, 1}, {0});
  compare_results(chip, {1, 0}, {0});
  compare_results(chip, {1, 1}, {1});
}

TEST_F(TestChips, And3) {
  hdlc::Chip chip(g_code, "And3");
  for (int x = 0; x < 7; x++) {
    char a = x & 1;
    char b = (x >> 1) & 1;
    char c = (x >> 2) & 1;
    compare_results(chip, {a, b, c}, {a && b && c});
  }
}