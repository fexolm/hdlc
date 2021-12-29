#pragma once

#include "ast.h"
#include "parser_error.h"
#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace hdlc::ast {
std::shared_ptr<Package> parse_package(const std::string &data,
                                       const std::string &name);
}