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
  ast::Printer p(ss);
  p.visit(*pkg);

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