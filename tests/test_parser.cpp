#include "gtest/gtest.h"
#include "hdlc/parser.h"
#include "hdlc/parser_error.h"
#include "gtest_util.h"
#include <sstream>

TEST(ParsePackage, CorrectInput) {
    std::string reference = R"(
chip And (a wire, b wire) wire {
    tmp := Nand(a, b)
    res := Nand(tmp, tmp)
    return res
}

chip And3(a wire, b wire, c wire) wire {
	tmp := And(a, b)
	res := And(tmp, c)
	return res
})";

    std::string expectedValue = R"(Package: test_pkg

chip And (a, b, ) wire, {
    tmp, := Nand(a, b, )
    res, := Nand(tmp, tmp, )
    return res, 
}

chip And3 (a, b, c, ) wire, {
    tmp, := And(a, b, )
    res, := And(tmp, c, )
    return res, 
}

)";

    auto pkg = ast::parse_package(reference, "test_pkg");
    std::stringstream ss;
    ast::Printer p(ss);
    p.visit(*pkg);

    EXPECT_EQ(expectedValue, ss.str());
}

TEST(ParsePackage, IncorrectInput){
    std::string reference = R"(
chip And (a wire, b ) wire {
return a
}
)";
    
    EXPECT_THROW_WITH_MESSAGE(ast::parse_package(reference, "test_pkg"), ParserError, "Parser error: ident should start with letter or _ (line 1, pos 20)");
}
