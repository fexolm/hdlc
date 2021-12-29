#pragma once

#include <exception>
#include <sstream>

namespace hdlc::ast {
class ParserError : public std::exception {
private:
  std::string message;

public:
  ParserError(std::string msg, size_t line, size_t line_pos);
  const char *what() const noexcept override { return message.c_str(); }
};
}