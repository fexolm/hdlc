#include "hdlc/parser.h"
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    std::ifstream ifs("code.txt");
    std::string code((std::istreambuf_iterator<char>(ifs)),
                     (std::istreambuf_iterator<char>()));

    Parser p(code);
    try
    {
        auto pkg = p.read_package("gates");
        ast::Printer v(std::cout);
        v.visit(*pkg);
    }
    catch (ParserError& e)
    {
        std::cout << e.what() << std::endl;
        throw;
    }
}
