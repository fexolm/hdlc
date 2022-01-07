#include "gtest_util.h"
#include "hdlc/ast/parser.h"
#include "hdlc/ast/parser_error.h"
#include "gtest/gtest.h"
#include <sstream>

using namespace hdlc;

TEST(ParsePackage, CorrectInput) {
  std::string code = R"(
chip And (a, b) res {
    tmp := Nand(a, b)
    res := Nand(tmp, tmp)
    return res
}

chip And3(a, b, c) res {
	tmp := And(a, b)
	res := And(tmp, c)
	return res
})";

  std::string expected_result = R"(Package: test_pkg

chip And (a, b, ) res, {
    tmp, := Nand(a, b, )
    res, := Nand(tmp, tmp, )
    return res, 
}

chip And3 (a, b, c, ) res, {
    tmp, := And(a, b, )
    res, := And(tmp, c, )
    return res, 
}

)";

  auto pkg = ast::parse_package(code, "test_pkg");
  std::stringstream ss;
  ast::print_package(ss, pkg);

  EXPECT_EQ(expected_result, ss.str());
}

TEST(ParsePackage, IncorrectReturnStatement) {
  std::string code = R"(
chip And (a, b) wire {
return 
}
)";

  EXPECT_THROW_WITH_MESSAGE(
      ast::parse_package(code, "test_pkg"), ast::ParserError,
      "Parser error: ident should start with letter or _ (line 3, pos 0)");
}

TEST(ParsePackage, IdentFuncNamesError) {
  std::string code = R"(
chip And (a, b) res {
    tmp := Nand(a, b)
    res := Nand(tmp, tmp)
    return res
}

chip And(a, b, c) res {
	tmp := And(a, b)
	res := And(tmp, c)
	return res
})";

  EXPECT_THROW_WITH_MESSAGE(
      ast::parse_package(code, "test_pkg"), ast::ParserError,
      "Parser error: chip with name And already declared (line 7, pos 0)");
}

TEST(ParserPackage, HaveNoReturnError) {
  GTEST_SKIP();
  std::string code = R"(
chip And (a, b) res {
    tmp := Nand(a, b)
    res := Nand(tmp, tmp)
}
})";
  EXPECT_THROW_WITH_MESSAGE(
      ast::parse_package(code, "test_pkg"), ast::ParserError,
      "Parser error: no return statement found (line 3, pos 0)");
}

TEST(ParserPackage, NoInputsError) {
  GTEST_SKIP();
  std::string code = R"(
chip And () res {
    tmp := Nand(a, b)
    res := Nand(tmp, tmp)
    return a
}
})";
  EXPECT_THROW_WITH_MESSAGE(
      ast::parse_package(code, "test_pkg"), ast::ParserError,
      "Parser error: no input statements found (line 1, pos 11)");
}

TEST(ParserPackage, NoOutputsError) {
  GTEST_SKIP();
  std::string code = R"(
chip And (a, b) {
    tmp := Nand(a, b)
    res := Nand(tmp, tmp)
    return a
}
})";
  EXPECT_THROW_WITH_MESSAGE(
      ast::parse_package(code, "test_pkg"), ast::ParserError,
      "Parser error: no output statements found (line 1, pos 17)");
}

TEST(ParsePackage, Slices) {
  std::string code = R"(
chip And (a, b) res {
    tmp := Nand(a, b)
    res := Nand(tmp, tmp)
    return res
}

chip And4Way (a[4], b[4]) res[4] {
    return [
      And(a[0], b[0]),
      And(a[1], b[1]),
      And(a[2], b[2]),
      And(a[3], b[3])
    ]
})";

  std::string expected_result = R"(Package: test_pkg

chip And (a, b, ) res, {
    tmp, := Nand(a, b, )
    res, := Nand(tmp, tmp, )
    return res, 
}

chip And4Way (a[4], b[4], ) res[4], {
    return [*And(*a[0:1], *b[0:1], ),*And(*a[1:2], *b[1:2], ),*And(*a[2:3], *b[2:3], ),*And(*a[3:4], *b[3:4], ),], 
}

)";

  auto pkg = ast::parse_package(code, "test_pkg");
  std::stringstream ss;
  ast::print_package(ss, pkg);

  EXPECT_EQ(expected_result, ss.str());
}

TEST(RegistersTest, CreateAssignReadNoSlices) {
  std::string code = R"(
chip Prev (a) res {
    r := Register()
    r <- a
    return <- r
}
)";

  std::string expected_result = R"(Package: test_pkg

chip Prev (a, ) res, {
    r, := Register()
    r <- a
    return <- r, 
}

)";

  auto pkg = ast::parse_package(code, "test_pkg");
  std::stringstream ss;
  ast::print_package(ss, pkg);

  EXPECT_EQ(expected_result, ss.str());
}

TEST(RegistersTest, MultipleAssignee) {
  GTEST_SKIP();
  std::string code = R"(
chip Prev (a) res {
    r1, r2 := Register()
    return <- r1, <- r2
}
)";

  EXPECT_THROW_WITH_MESSAGE(ast::parse_package(code, "test_pkg"),
                            ast::ParserError,
                            "Parser error: unable to assign to multiple "
                            "registers in single statement (line 2, pos 4)");
}
