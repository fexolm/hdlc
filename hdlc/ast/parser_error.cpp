#include "parser_error.h"

namespace hdlc::ast {
ParserError::ParserError(std::string msg, size_t line, size_t line_pos) {
  std::stringstream ss;
  ss << "Parser error: " << msg << " (line " << line << ", pos " << line_pos
     << ")";
  message = ss.str();
}
} // namespace hdlc::ast