#pragma once

#include "ast.h"
#include "parser_error.h"
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

using SymbolMap = std::unordered_map<std::string, std::shared_ptr<ast::Value>>;

class Parser {
private:
  std::string data;
  size_t pos;
  size_t line;
  size_t line_pos;

  std::vector<size_t> line_length;

public:
  explicit Parser(std::string data) : data(std::move(data)), pos(0) {}

  std::shared_ptr<ast::Package> read_package(std::string name);

private:
  char read_symbol();

  void move_to(size_t new_pos);

  char peek_symbol();

  template <typename P> size_t get_end_pos(P predicate);

  void skip_spaces();

  template <typename P> std::string_view peak_symbol_sequence(P predicate);

  template <typename P> std::string_view read_symbol_sequence(P predicate);

  std::string_view read_ident();

  std::string_view read_string();

  std::string_view peek_string();

  void expect_symbol_sequence(std::string_view seq);

  std::shared_ptr<ast::Chip> read_chip();

  std::vector<std::shared_ptr<ast::Value>> read_params();

  std::shared_ptr<ast::Value> read_value();

  std::vector<std::shared_ptr<ast::Type>> read_return_types();

  std::vector<std::shared_ptr<ast::Stmt>> read_chip_body(SymbolMap &symbol_map);

  std::shared_ptr<ast::Stmt> read_stmt(SymbolMap &symbol_map);

  std::shared_ptr<ast::RegWrite> read_reg_write(SymbolMap &symbol_map);

  std::shared_ptr<ast::RegInit> read_reg_init(SymbolMap &symbol_map);

  std::shared_ptr<ast::RetStmt> read_ret_stmt(SymbolMap &symbol_map);

  std::shared_ptr<ast::AssignStmt> read_assign_stmt(SymbolMap &symbol_map);

  std::shared_ptr<ast::Expr> read_expr(SymbolMap &symbol_map);

  std::shared_ptr<ast::RegRead> read_reg_read(SymbolMap &symbol_map);

  std::vector<std::shared_ptr<ast::Expr>> read_expr_list(SymbolMap &symbol_map);

  std::vector<std::shared_ptr<ast::Value>>
  read_ident_list(SymbolMap &symbol_map);
};
