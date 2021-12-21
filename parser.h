#include "ast.h"
#include <exception>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

class ParserError : std::exception {
private:
  std::string message;

public:
  ParserError(std::string msg, size_t line, size_t line_pos) {
    std::stringstream ss;
    ss << "Parser error: " << msg << " (line " << line << ", pos " << line_pos
       << ")" << std::endl;
    message = ss.str();
  }

  const char *what() const noexcept override { return message.c_str(); }
};

class Parser {
private:
  std::string data;
  size_t pos;
  size_t line;
  size_t line_pos;

public:
  explicit Parser(std::string data) : data(std::move(data)), pos(0) {}

  std::shared_ptr<ast::Package> read_package(std::string name) {
    auto res = std::make_shared<ast::Package>();
    res->name = name;
    skip_spaces();

    while (peek_symbol() != -1) {
      auto chip = read_chip();
      res->chips.push_back(chip);
      skip_spaces();
    }

    return res;
  }

private:
  char read_symbol() {
    if (pos < data.size()) {
      auto sym = data[pos];
      if (sym == '\n') {
        line++;
        line_pos = 0;
      } else {
        line_pos++;
      }
      pos++;
      return sym;
    }
    return -1;
  }

  void skip_to(size_t new_pos) {
    for (; pos < new_pos; read_symbol())
      ;
  }

  char peek_symbol() {
    if (pos < data.size()) {
      return data[pos];
    }
    return -1;
  }

  template <typename P> size_t get_end_pos(P predicate) {
    size_t p = pos;
    for (; p < data.size() && predicate(data[p]); p++)
      ;
    return p;
  }

  void skip_spaces() {
    skip_to(get_end_pos([](char c) { return std::isspace(c); }));
  }

  template <typename P> std::string_view peak_symbol_sequence(P predicate) {
    auto end = get_end_pos([&predicate](char c) { return predicate(c); });
    auto len = end - pos;
    return std::string_view(data.c_str() + pos, len);
  }

  template <typename P> std::string_view read_symbol_sequence(P predicate) {
    auto res = peak_symbol_sequence(predicate);
    skip_to(pos + res.size());
    return res;
  }

  std::string_view read_ident() {
    auto first_symbol = peek_symbol();
    if (!std::isalpha(first_symbol) && first_symbol != '_') {
      throw ParserError("ident should start with letter or _", line, line_pos);
    }
    return read_symbol_sequence(
        [](char c) { return std::isalpha(c) || std::isdigit(c) || c == '_'; });
  }

  std::string_view read_string() {
    return read_symbol_sequence([](char c) { return std::isalpha(c); });
  }

  std::string_view peek_string() {
    return peak_symbol_sequence([](char c) { return std::isalpha(c); });
  }

  void expect_symbol_sequence(std::string_view seq) {
    for (int i = 0; i < seq.size(); ++i) {
      if (read_symbol() != seq[i]) {
        throw ParserError(std::string("expected ") + seq.data() + " keryword",
                          line, line_pos);
      }
    }
  }

  std::shared_ptr<ast::Chip> read_chip() {
    expect_symbol_sequence("chip"); // TODO: check space
    skip_spaces();

    auto name = read_ident();
    skip_spaces();
    expect_symbol_sequence("(");

    skip_spaces();

    auto params = read_params();

    skip_spaces();

    expect_symbol_sequence(")");

    skip_spaces();

    auto results = read_return_types();

    skip_spaces();

    auto body = read_chip_body();

    auto chip = std::make_shared<ast::Chip>();
    chip->ident = name;
    chip->inputs = params;
    chip->outputs = results;
    chip->body = body;

    return chip;
  }

  std::vector<std::shared_ptr<ast::Value>> read_params() {
    std::vector<std::shared_ptr<ast::Value>> res;

    while (true) {
      auto val = read_value();
      res.push_back(val);
      skip_spaces();
      if (peek_symbol() == ',') {
        read_symbol();
        skip_spaces();
      } else {
        break;
      }
    }
    return res;
  }

  std::shared_ptr<ast::Value> read_value() {
    auto name = read_ident();
    skip_spaces();
    std::string type(read_ident());

    auto res = std::make_shared<ast::Value>();
    res->ident = name;
    res->type = std::make_shared<ast::Type>(type);

    return res;
  }

  std::vector<std::shared_ptr<ast::Type>> read_return_types() {
    std::vector<std::shared_ptr<ast::Type>> res;

    while (true) {
      std::string type(read_string());
      res.push_back(std::make_shared<ast::Type>(type));
      skip_spaces();
      if (peek_symbol() == ',') {
        read_symbol();
        skip_spaces();
      } else {
        break;
      }
    }

    return res;
  }

  std::vector<std::shared_ptr<ast::Stmt>> read_chip_body() {
    std::vector<std::shared_ptr<ast::Stmt>> res;

    expect_symbol_sequence("{");
    skip_spaces();

    while (peek_symbol() != '}') {
      auto stmt = read_stmt();
      res.push_back(stmt);
      skip_spaces();
    }

    expect_symbol_sequence("}");

    return res;
  }

  std::shared_ptr<ast::Stmt> read_stmt() {
    auto w = peek_string();

    if (w == "return") {
      return read_ret_stmt();
    } else {
      return read_assign_stmt();
    }
  }

  std::shared_ptr<ast::RetStmt> read_ret_stmt() {
    expect_symbol_sequence("return");
    skip_spaces();

    auto res = std::make_shared<ast::RetStmt>();
    res->results = read_expr_list();
    return res;
  }

  std::shared_ptr<ast::AssignStmt> read_assign_stmt() {
    auto res = std::make_shared<ast::AssignStmt>();
    res->assignees = read_ident_list();
    skip_spaces();
    expect_symbol_sequence(":=");
    skip_spaces();
    res->rhs = read_expr();
    return res;
  }

  std::shared_ptr<ast::Expr> read_expr() {
    std::string ident(read_ident());
    skip_spaces();
    if (peek_symbol() == '(') {
      expect_symbol_sequence("(");
      skip_spaces();
      auto params = read_expr_list();
      skip_spaces();
      expect_symbol_sequence(")");
      return std::make_shared<ast::CallExpr>(ident, params);
    }

    return std::make_shared<ast::Value>(ident, nullptr);
  }

  std::vector<std::shared_ptr<ast::Expr>> read_expr_list() {
    std::vector<std::shared_ptr<ast::Expr>> res;

    while (true) {
      res.push_back(read_expr());
      skip_spaces();
      if (peek_symbol() == ',') {
        read_symbol();
        skip_spaces();
      } else {
        break;
      }
    }

    return res;
  }

  std::vector<std::shared_ptr<ast::Value>> read_ident_list() {
    std::vector<std::shared_ptr<ast::Value>> res;

    while (true) {
      res.emplace_back(
          std::make_shared<ast::Value>(std::string(read_ident()), nullptr));
      skip_spaces();
      if (peek_symbol() == ',') {
        read_symbol();
        skip_spaces();
      } else {
        break;
      }
    }

    return res;
  }
};