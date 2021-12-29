#pragma once

#include "jit/module.h"
#include <memory>
#include <string>
#include <vector>

namespace hdlc {
class Slot {
private:
  bool *ptr;

public:
  explicit Slot(bool *ptr);

  Slot(const Slot &) = delete;

  void set(bool val);

  bool get() const;
};

class Chip {
private:
  std::unique_ptr<jit::Module> module;

  std::vector<char> inputs;
  std::vector<char> outputs;

public:
  explicit Chip(const std::string &path, const std::string &chip_name);

  size_t get_num_input_slots();

  size_t get_num_output_slots();

  Slot get_input_slot(size_t i);

  const Slot get_output_slot(size_t i);

  void run();
};
} // namespace hdlc
