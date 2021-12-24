#pragma once

#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace ast
{

struct Chip;
struct ChipSignature;
struct ChipBody;
struct Stmt;
struct Expr;
struct Value;
struct Type;
struct Visitor;
struct Package;
struct AssignStmt;
struct CallExpr;
struct RetStmt;
struct RegInit;
struct RegWrite;
struct RegRead;

struct Node
{
    virtual void visit(Visitor& v) = 0;
};

struct Visitor
{
    virtual void visit(Package& pkg) = 0;
    virtual void visit(Chip& chip) = 0;
    virtual void visit(AssignStmt& stmt) = 0;
    virtual void visit(CallExpr& expr) = 0;
    virtual void visit(Value& val) = 0;
    virtual void visit(RetStmt& stmt) = 0;
    virtual void visit(RegInit& reg) = 0;
    virtual void visit(RegWrite& rw) = 0;
    virtual void visit(RegRead& rr) = 0;
};

struct Package : Node
{
    std::string name;
    std::vector<std::shared_ptr<Chip>> chips;

    void visit(Visitor& v) override { v.visit(*this); };
};

struct Chip : Node
{
    std::string ident;
    std::vector<std::shared_ptr<Value>> inputs;
    std::vector<std::shared_ptr<Type>> outputs;
    std::vector<std::shared_ptr<Stmt>> body;

    void visit(Visitor& v) override { v.visit(*this); }
};

struct Type
{
    std::string name;

    explicit Type(const std::string& name) : name(name) {}
};

struct Stmt : Node
{
};

struct AssignStmt : Stmt
{
    std::vector<std::shared_ptr<Value>> assignees;
    std::shared_ptr<Expr> rhs;

    void visit(Visitor& v) override { v.visit(*this); }
};

struct Expr : Node
{
};

struct CallExpr : Expr
{
    std::string chip_name;
    std::vector<std::shared_ptr<Expr>> args;

    CallExpr(std::string name, std::vector<std::shared_ptr<Expr>> args)
        : chip_name(name), args(args)
    {
    }

    void visit(Visitor& v) override { v.visit(*this); }
};

struct Value : Expr
{
    std::string ident;
    std::shared_ptr<Type> type;

    Value(std::string name, std::shared_ptr<Type> type)
        : ident(name), type(type)
    {
    }

    Value() {}

    void visit(Visitor& v) override { v.visit(*this); }
};

struct RetStmt : Stmt
{
    std::vector<std::shared_ptr<Expr>> results;

    void visit(Visitor& v) override { v.visit(*this); }
};

struct RegInit : Stmt
{
    std::shared_ptr<Value> reg;

    explicit RegInit(std::shared_ptr<Value> reg) : reg(reg) {}

    void visit(Visitor& v) override { v.visit(*this); }
};

struct RegWrite : Stmt
{
    std::shared_ptr<Value> reg;
    std::shared_ptr<Expr> rhs;

    explicit RegWrite(std::shared_ptr<Value> reg, std::shared_ptr<Expr> rhs)
        : reg(reg), rhs(rhs)
    {
    }

    void visit(Visitor& v) override { v.visit(*this); }
};

struct RegRead : Expr
{
    std::shared_ptr<Value> reg;

    explicit RegRead(std::shared_ptr<Value> reg) : reg(reg) {}

    void visit(Visitor& v) override { v.visit(*this); }
};

struct Printer : Visitor
{
    std::ostream& out;

    Printer(std::ostream& out) : out(out) {}
    void visit(Package& pkg)
    {
        out << "Package: " << pkg.name << "\n\n";
        for (auto& c : pkg.chips)
        {
            c->visit(*this);
        }
    }
    void visit(Chip& chip)
    {
        out << "chip " << chip.ident << " (";
        for (auto& i : chip.inputs)
        {
            i->visit(*this);
            out << ", ";
        }
        out << ") ";

        for (auto& o : chip.outputs)
        {
            out << o->name << ", ";
        }
        out << "{" << std::endl;

        for (auto& s : chip.body)
        {
            s->visit(*this);
            out << std::endl;
        }
        out << "}"
            << "\n\n";
    }

    void visit(Value& val) { out << val.ident; }
    void visit(AssignStmt& stmt)
    {
        out << "    ";
        for (auto& v : stmt.assignees)
        {
            v->visit(*this);
            out << ", ";
        }
        out << ":= ";

        stmt.rhs->visit(*this);
    }
    void visit(CallExpr& expr)
    {
        out << expr.chip_name << "(";
        for (auto& arg : expr.args)
        {
            arg->visit(*this);
            out << ", ";
        }
        out << ")";
    }
    void visit(RetStmt& stmt)
    {
        out << "    return ";
        for (auto& v : stmt.results)
        {
            v->visit(*this);
            out << ", ";
        }
    }

    void visit(RegInit& init)
    {
        out << "    reg ";
        init.reg->visit(*this);
    }

    void visit(RegWrite& rw)
    {
        out << "    ";
        rw.reg->visit(*this);
        out << " <- ";
        rw.rhs->visit(*this);
    }

    void visit(RegRead& rr)
    {
        out << "<- ";
        rr.reg->visit(*this);
    }
};
} // namespace ast
