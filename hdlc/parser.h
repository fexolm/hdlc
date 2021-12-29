#pragma once

#include "ast.h"
#include "parser_error.h"
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ast {
std::shared_ptr<ast::Package> parse_package(const std::string &data,
                                            const std::string &name);
}