#pragma once
#include "ast.h"
#include <memory>

namespace hdlc::ast {
void insert_casts(std::shared_ptr<Package> pakage);
}