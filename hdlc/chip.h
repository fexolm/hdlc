#pragma once

#include <memory>
#include <string>

namespace hdlc {

struct Chip {
  virtual void run(int8_t *intputs, int8_t *outputs) = 0;
  virtual ~Chip() = default;
};

std::shared_ptr<Chip> create_chip(const std::string &code,
                                  const std::string &chip_name);
} // namespace hdlc
