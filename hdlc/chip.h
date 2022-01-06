#pragma once

#include "jit/module.h"
#include <memory>
#include <string>
#include <vector>

namespace hdlc {

class Chip {
private:
  std::unique_ptr<jit::Module> module;

public:
  explicit Chip(const std::string &code, const std::string &chip_name);

  void run(int8_t *intputs, int8_t *outputs);
};
} // namespace hdlc
