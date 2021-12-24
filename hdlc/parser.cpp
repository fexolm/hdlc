#include "parser.h"

std::shared_ptr<ast::Package> Parser::read_package(std::string name)
{
    auto res = std::make_shared<ast::Package>();
    res->name = name;
    skip_spaces();

    while (peek_symbol() != -1)
    {
        auto chip = read_chip();
        res->chips.push_back(chip);
        skip_spaces();
    }

    return res;
}

char Parser::read_symbol()
{
    if (pos < data.size())
    {
        auto sym = data[pos];
        if (sym == '\n')
        {
            if (line_length.size() < line)
            {
                line_length.push_back(line_pos);
            }
            line++;
            line_pos = 0;
        }
        else
        {
            line_pos++;
        }
        pos++;
        return sym;
    }
    return -1;
}

void Parser::move_to(size_t new_pos)
{
    if (pos <= new_pos)
    {
        for (; pos < new_pos; read_symbol())
            ;
    }
    else
    {
        for (; pos >= 0 && pos > new_pos; --pos)
        {
            if (data[pos] == '\n')
            {
                line--;
                line_pos = line_length[line];
            }
            else
            {
                line_pos--;
            }
        }
    }
}

char Parser::peek_symbol()
{
    if (pos < data.size())
    {
        return data[pos];
    }
    return -1;
}

template <typename P>
size_t Parser::get_end_pos(P predicate)
{
    size_t p = pos;
    for (; p < data.size() && predicate(data[p]); p++)
        ;
    return p;
}

void Parser::skip_spaces()
{
    move_to(get_end_pos([](char c) { return std::isspace(c); }));
}

template <typename P>
std::string_view Parser::peak_symbol_sequence(P predicate)
{
    auto end = get_end_pos([&predicate](char c) { return predicate(c); });
    auto len = end - pos;
    return std::string_view(data.c_str() + pos, len);
}

template <typename P>
std::string_view Parser::read_symbol_sequence(P predicate)
{
    auto res = peak_symbol_sequence(predicate);
    move_to(pos + res.size());
    return res;
}

std::string_view Parser::read_ident()
{
    auto first_symbol = peek_symbol();
    if (!std::isalpha(first_symbol) && first_symbol != '_')
    {
        throw ParserError("ident should start with letter or _", line,
                          line_pos);
    }
    return Parser::read_symbol_sequence(
        [](char c) { return std::isalpha(c) || std::isdigit(c) || c == '_'; });
}

std::string_view Parser::read_string()
{
    return read_symbol_sequence([](char c) { return std::isalpha(c); });
}

std::string_view Parser::peek_string()
{
    return peak_symbol_sequence([](char c) { return std::isalpha(c); });
}

void Parser::expect_symbol_sequence(std::string_view seq)
{
    for (int i = 0; i < seq.size(); ++i)
    {
        if (read_symbol() != seq[i])
        {
            throw ParserError(std::string("expected ") + seq.data() +
                                  " keryword",
                              line, line_pos);
        }
    }
}

std::shared_ptr<ast::Chip> Parser::read_chip()
{
    expect_symbol_sequence("chip"); // TODO: check space
    skip_spaces();

    auto name = read_ident();
    skip_spaces();
    expect_symbol_sequence("(");

    skip_spaces();

    SymbolMap local_vars;

    auto params = read_params();

    for (auto& p : params)
    {
        local_vars[p->ident] = p;
    }

    skip_spaces();

    expect_symbol_sequence(")");

    skip_spaces();

    auto results = read_return_types();

    skip_spaces();

    auto body = read_chip_body(local_vars);

    auto chip = std::make_shared<ast::Chip>();
    chip->ident = name;
    chip->inputs = params;
    chip->outputs = results;
    chip->body = body;

    return chip;
}

std::vector<std::shared_ptr<ast::Value>> Parser::read_params()
{
    std::vector<std::shared_ptr<ast::Value>> res;
    if (peek_symbol() == ')')
    {
        return res;
    }
    while (true)
    {
        auto val = read_value();
        res.push_back(val);
        skip_spaces();
        if (peek_symbol() == ',')
        {
            read_symbol();
            skip_spaces();
        }
        else
        {
            break;
        }
    }
    return res;
}

std::shared_ptr<ast::Value> Parser::read_value()
{
    auto name = read_ident();
    skip_spaces();
    std::string type(read_ident());

    auto res = std::make_shared<ast::Value>();
    res->ident = name;
    res->type = std::make_shared<ast::Type>(type);

    return res;
}

std::vector<std::shared_ptr<ast::Type>> Parser::read_return_types()
{
    std::vector<std::shared_ptr<ast::Type>> res;

    if (peek_symbol() == '{')
    {
        return res;
    }

    while (true)
    {
        std::string type(read_string());
        res.push_back(std::make_shared<ast::Type>(type));
        skip_spaces();
        if (peek_symbol() == ',')
        {
            read_symbol();
            skip_spaces();
        }
        else
        {
            break;
        }
    }

    return res;
}

std::vector<std::shared_ptr<ast::Stmt>>
Parser::read_chip_body(SymbolMap& symbol_map)
{
    std::vector<std::shared_ptr<ast::Stmt>> res;

    expect_symbol_sequence("{");
    skip_spaces();

    while (peek_symbol() != '}')
    {
        auto stmt = read_stmt(symbol_map);
        res.push_back(stmt);
        skip_spaces();
    }

    expect_symbol_sequence("}");

    return res;
}

std::shared_ptr<ast::Stmt> Parser::read_stmt(SymbolMap& symbol_map)
{
    auto w = peek_string();

    if (w == "return")
    {
        return read_ret_stmt(symbol_map);
    }
    else if (w == "reg")
    {
        return read_reg_init(symbol_map);
    }
    else
    {

        // TODO: add regWriteStmt support
        auto saved_pos = pos;
        try
        {
            return read_assign_stmt(symbol_map);
        }
        catch (ParserError& e)
        {
            move_to(saved_pos);
            return read_reg_write(symbol_map);
        }
    }
}

std::shared_ptr<ast::RegWrite> Parser::read_reg_write(SymbolMap& symbol_map)
{
    std::string reg_name(read_ident());

    if (!symbol_map.count(reg_name))
    {
        throw ParserError("Register with name " + reg_name +
                              " was not initialized",
                          line, line_pos);
    }
    auto reg = symbol_map[reg_name];
    skip_spaces();
    expect_symbol_sequence("<-");
    skip_spaces();
    auto rhs = read_expr(symbol_map);
    return std::make_shared<ast::RegWrite>(reg, rhs);
}

std::shared_ptr<ast::RegInit> Parser::read_reg_init(SymbolMap& symbol_map)
{
    expect_symbol_sequence("reg");
    skip_spaces();
    std::string name(read_ident());
    if (symbol_map.count(name))
    {
        throw ParserError("Creating register with existing name", line,
                          line_pos);
    }

    symbol_map[name] = std::make_shared<ast::Value>(name, nullptr);
    return std::make_shared<ast::RegInit>(symbol_map[name]);
}

std::shared_ptr<ast::RetStmt> Parser::read_ret_stmt(SymbolMap& symbol_map)
{
    expect_symbol_sequence("return");
    skip_spaces();

    auto res = std::make_shared<ast::RetStmt>();
    res->results = read_expr_list(symbol_map);
    return res;
}

std::shared_ptr<ast::AssignStmt> Parser::read_assign_stmt(SymbolMap& symbol_map)
{
    auto res = std::make_shared<ast::AssignStmt>();
    res->assignees = read_ident_list(symbol_map);
    skip_spaces();
    expect_symbol_sequence(":=");
    skip_spaces();
    res->rhs = read_expr(symbol_map);
    return res;
}

std::shared_ptr<ast::Expr> Parser::read_expr(SymbolMap& symbol_map)
{
    if (peek_symbol() == '<')
    {
        return read_reg_read(symbol_map);
    }

    std::string ident(read_ident());
    skip_spaces();
    if (peek_symbol() == '(')
    {
        expect_symbol_sequence("(");
        skip_spaces();
        auto params = read_expr_list(symbol_map);
        skip_spaces();
        expect_symbol_sequence(")");
        return std::make_shared<ast::CallExpr>(ident, params);
    }

    if (!symbol_map.count(ident))
    {
        throw ParserError("Referenced variable not initialized", line,
                          line_pos);
    }
    return symbol_map[ident];
}

std::shared_ptr<ast::RegRead> Parser::read_reg_read(SymbolMap& symbol_map)
{
    expect_symbol_sequence("<-");
    skip_spaces();
    std::string ident(read_ident());

    if (!symbol_map.count(ident))
    {
        throw ParserError("Referenced variable not initialized", line,
                          line_pos);
    }

    return std::make_shared<ast::RegRead>(symbol_map[ident]);
}

std::vector<std::shared_ptr<ast::Expr>>
Parser::read_expr_list(SymbolMap& symbol_map)
{
    std::vector<std::shared_ptr<ast::Expr>> res;

    while (true)
    {
        res.push_back(read_expr(symbol_map));
        skip_spaces();
        if (peek_symbol() == ',')
        {
            read_symbol();
            skip_spaces();
        }
        else
        {
            break;
        }
    }

    return res;
}

std::vector<std::shared_ptr<ast::Value>>
Parser::read_ident_list(SymbolMap& symbol_map)
{
    std::vector<std::shared_ptr<ast::Value>> res;

    while (true)
    {
        auto val =
            std::make_shared<ast::Value>(std::string(read_ident()), nullptr);

        if (symbol_map.count(val->ident))
        {
            throw ParserError("Multiple assign to local variable", line,
                              line_pos);
        }

        res.emplace_back(val);
        symbol_map[val->ident] = val;

        skip_spaces();
        if (peek_symbol() == ',')
        {
            read_symbol();
            skip_spaces();
        }
        else
        {
            break;
        }
    }

    return res;
}