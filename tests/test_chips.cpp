#include "gtest_util.h"
#include "hdlc/ast/parser.h"
#include "hdlc/ast/parser_error.h"
#include "hdlc/chip.h"
#include "gtest/gtest.h"

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

chip StrangeAnd2Way(a[2], b[2]) res[2] {
  tmp := And4Way([a[0], a[1], a[0], a[1]], [b[0], b[1], b[0], b[1]])
  return tmp[0:2]
}

chip ArrayOfOne(a[1]) res[1] {
  return a
}

chip CallArrayOfOne(a) res {
  tmp:= ArrayOfOne([a])
  return tmp[0]
}

chip Prev(a) res {
  r := Register()
  r <- a
  return <- r
}

chip PrevSlice(a[4]) res[4] {
  r := Register(4)
  r <- a
  return <- r
}

chip PrevSlice8(a[8]) res[8] {
  p1 := PrevSlice(a[0:4])
  p2 := PrevSlice(a[4:8])

  return [p1[0], p1[1], p1[2], p1[3], p2[0], p2[1], p2[2], p2[3]]
}
)";

class TestChips : public ::testing::Test {
protected:
  void compare_results(hdlc::Chip &chip, std::vector<int8_t> inputs,
                       std::vector<int8_t> expected_outputs) {
    std::vector<int8_t> real_outputs(expected_outputs.size());
    chip.run(inputs.data(), real_outputs.data());

    for (size_t i = 0; i < real_outputs.size(); ++i) {
      EXPECT_EQ(real_outputs[i], expected_outputs[i]);
    }
  }
};

TEST_F(TestChips, And) {
  auto chip = hdlc::create_chip(g_code, "And");
  compare_results(*chip, {0, 0}, {0});
  compare_results(*chip, {0, 1}, {0});
  compare_results(*chip, {1, 0}, {0});
  compare_results(*chip, {1, 1}, {1});
}

TEST_F(TestChips, And3) {
  auto chip = hdlc::create_chip(g_code, "And3");
  for (size_t x = 0; x < 7; x++) {
    char a = x & 1;
    char b = (x >> 1) & 1;
    char c = (x >> 2) & 1;
    compare_results(*chip, {a, b, c}, {a && b && c});
  }
}

TEST_F(TestChips, And4Way) {
  auto chip = hdlc::create_chip(g_code, "And4Way");
  for (int x = 0; x < 16; ++x) {
    for (int y = 0; y < 16; ++y) {
      auto res = x & y;
      std::vector<int8_t> inputs;
      std::vector<int8_t> outputs;

      for (int offset = 0; offset < 4; ++offset) {
        inputs.push_back((x >> offset) & 1);
      }
      for (int offset = 0; offset < 4; ++offset) {
        inputs.push_back((y >> offset) & 1);
      }

      for (int offset = 0; offset < 4; ++offset) {
        outputs.push_back((res >> offset) & 1);
      }

      compare_results(*chip, inputs, outputs);
    }
  }
}

TEST_F(TestChips, StrangeAnd2Way) {
  auto chip = hdlc::create_chip(g_code, "StrangeAnd2Way");
  for (int x = 0; x < 4; ++x) {
    for (int y = 0; y < 4; ++y) {
      auto res = x & y;
      std::vector<int8_t> inputs;
      std::vector<int8_t> outputs;

      for (int offset = 0; offset < 2; ++offset) {
        inputs.push_back((x >> offset) & 1);
      }
      for (int offset = 0; offset < 2; ++offset) {
        inputs.push_back((y >> offset) & 1);
      }

      for (int offset = 0; offset < 2; ++offset) {
        outputs.push_back((res >> offset) & 1);
      }

      compare_results(*chip, inputs, outputs);
    }
  }
}

TEST_F(TestChips, ArrayOfOne) {
  auto chip = hdlc::create_chip(g_code, "CallArrayOfOne");
  compare_results(*chip, {0}, {0});
  compare_results(*chip, {1}, {1});
}

TEST_F(TestChips, Prev) {
  auto chip = hdlc::create_chip(g_code, "Prev");
  compare_results(*chip, {1}, {0});
  compare_results(*chip, {0}, {1});
  compare_results(*chip, {1}, {0});
  compare_results(*chip, {1}, {1});
}

TEST_F(TestChips, PrevSlice) {
  auto chip = hdlc::create_chip(g_code, "PrevSlice");
  compare_results(*chip, {1, 0, 1, 0}, {0, 0, 0, 0});
  compare_results(*chip, {0, 1, 1, 1}, {1, 0, 1, 0});
  compare_results(*chip, {1, 1, 1, 1}, {0, 1, 1, 1});
  compare_results(*chip, {0, 0, 0, 0}, {1, 1, 1, 1});
}

TEST_F(TestChips, PrevSlice8) {
  auto chip = hdlc::create_chip(g_code, "PrevSlice8");
  compare_results(*chip, {1, 0, 1, 0, 0, 1, 1, 1}, {0, 0, 0, 0, 0, 0, 0, 0});
  compare_results(*chip, {1, 0, 0, 1, 0, 0, 1, 1}, {1, 0, 1, 0, 0, 1, 1, 1});
  compare_results(*chip, {1, 1, 1, 0, 0, 1, 0, 0}, {1, 0, 0, 1, 0, 0, 1, 1});
  compare_results(*chip, {0, 0, 1, 0, 0, 0, 0, 1}, {1, 1, 1, 0, 0, 1, 0, 0});
}