#include "queijo/luau.h"

#include "Luau/Ast.h"
#include "Luau/Location.h"
#include "Luau/ParseResult.h"
#include "Luau/Parser.h"
#include "Luau/ParseOptions.h"
#include "Luau/ToString.h"

#include "lua.h"
#include "lualib.h"

namespace luau
{

static Luau::ParseResult parse(std::string& source)
{
    Luau::Allocator allocator;
    Luau::AstNameTable names{allocator};

    Luau::ParseOptions options;
    options.captureComments = true;
    options.allowDeclarationSyntax = false;

    Luau::ParseResult parseResult = Luau::Parser::parse(source.data(), source.size(), names, allocator, options);

    return parseResult;
}

struct ExprResult
{
    Luau::AstExpr* root;
    size_t lines = 0;

    std::vector<Luau::HotComment> hotcomments;
    std::vector<Luau::ParseError> errors;

    std::vector<Luau::Comment> commentLocations;
};

static ExprResult parseExpr(std::string& source)
{
    Luau::Allocator allocator;
    Luau::AstNameTable names{allocator};

    Luau::ParseOptions options;
    options.captureComments = true;
    options.allowDeclarationSyntax = false;

    Luau::Parser p(source.data(), source.size(), names, allocator, options);

    try
    {
        Luau::AstExpr* expr = p.parseExpr();
        size_t lines = p.lexer.current().location.end.line + (source.size() > 0 && source.data()[source.size() - 1] != '\n');

        return ExprResult{expr, lines, std::move(p.hotcomments), std::move(p.parseErrors), std::move(p.commentLocations)};
    }
    catch (Luau::ParseError& err)
    {
        // when catching a fatal error, append it to the list of non-fatal errors and return
        p.parseErrors.push_back(err);

        return ExprResult{nullptr, 0, {}, std::move(p.parseErrors)};
    }
}

struct AstSerialize : public Luau::AstVisitor
{
    lua_State* L;

    AstSerialize(lua_State* L)
        : L(L)
    {
    }

    void serialize(Luau::Position position)
    {
        lua_createtable(L, 0, 2);

        lua_pushnumber(L, position.line);
        lua_setfield(L, -2, "line");

        lua_pushnumber(L, position.column);
        lua_setfield(L, -2, "column");
    }

    void serialize(Luau::Location location)
    {
        lua_createtable(L, 0, 2);

        serialize(location.begin);
        lua_setfield(L, -2, "begin");

        serialize(location.end);
        lua_setfield(L, -2, "end");
    }

    void serialize(Luau::AstName name)
    {
        lua_createtable(L, 0, 1);

        lua_pushstring(L, name.value);
        lua_setfield(L, -2, "value");
    }

    void serialize(Luau::AstExprTable::Item& item)
    {
        lua_createtable(L, 0, 3);

        switch (item.kind)
        {
        case Luau::AstExprTable::Item::Kind::List:
            lua_pushstring(L, "list");
            break;
        case Luau::AstExprTable::Item::Kind::Record:
            lua_pushstring(L, "record");
            break;
        case Luau::AstExprTable::Item::Kind::General:
            lua_pushstring(L, "general");
            break;
        }
        lua_setfield(L, -2, "kind");

        visit(item.key);
        lua_setfield(L, -2, "key");

        visit(item.value);
        lua_setfield(L, -2, "value");
    }

    void withLocation(Luau::Location location)
    {
        serialize(location);
        lua_setfield(L, -2, "location");
    }

    void serialize(Luau::AstExprBinary::Op& op)
    {
        switch (op)
        {
        case Luau::AstExprBinary::Op::Add:
            lua_pushstring(L, "+");
            break;
        case Luau::AstExprBinary::Op::Sub:
            lua_pushstring(L, "-");
            break;
        case Luau::AstExprBinary::Op::Mul:
            lua_pushstring(L, "*");
            break;
        case Luau::AstExprBinary::Op::Div:
            lua_pushstring(L, "/");
            break;
        case Luau::AstExprBinary::Op::FloorDiv:
            lua_pushstring(L, "//");
            break;
        case Luau::AstExprBinary::Op::Mod:
            lua_pushstring(L, "%");
            break;
        case Luau::AstExprBinary::Op::Pow:
            lua_pushstring(L, "^");
            break;
        case Luau::AstExprBinary::Op::Concat:
            lua_pushstring(L, "..");
            break;
        case Luau::AstExprBinary::Op::CompareNe:
            lua_pushstring(L, "~=");
            break;
        case Luau::AstExprBinary::Op::CompareEq:
            lua_pushstring(L, "==");
            break;
        case Luau::AstExprBinary::Op::CompareLt:
            lua_pushstring(L, "<");
            break;
        case Luau::AstExprBinary::Op::CompareLe:
            lua_pushstring(L, "<=");
            break;
        case Luau::AstExprBinary::Op::CompareGt:
            lua_pushstring(L, ">");
            break;
        case Luau::AstExprBinary::Op::CompareGe:
            lua_pushstring(L, ">=");
            break;
        case Luau::AstExprBinary::Op::And:
            lua_pushstring(L, "and");
            break;
        case Luau::AstExprBinary::Op::Or:
            lua_pushstring(L, "or");
            break;
        case Luau::AstExprBinary::Op::Op__Count:
            luaL_error(L, "encountered illegal operator: Op__Count");
        }
    }

    // preambleSize should encode the size of the fields we're setting up for _all_ nodes.
    static const size_t preambleSize = 2;
    void serializeNodePreamble(Luau::AstNode* node, const char* tag)
    {
        lua_pushstring(L, tag);
        lua_setfield(L, -2, "tag");

        withLocation(node->location);
    }

    void serializeExprs(Luau::AstArray<Luau::AstExpr*>& exprs, size_t nrec = 0)
    {
        lua_createtable(L, exprs.size, nrec);

        for (size_t i = 0; i < exprs.size; i++)
        {
            exprs.data[i]->visit(this);
            lua_rawseti(L, -2, i + 1);
        }
    }

    void serializeStats(Luau::AstArray<Luau::AstStat*>& stats, size_t nrec = 0)
    {
        lua_createtable(L, stats.size, nrec);

        for (size_t i = 0; i < stats.size; i++)
        {
            stats.data[i]->visit(this);
            lua_rawseti(L, -2, i + 1);
        }
    }

    void serialize(Luau::AstExprGroup* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "group");

        node->expr->visit(this);
        lua_setfield(L, -2, "expr");
    }

    void serialize(Luau::AstExprConstantNil* node)
    {
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "nil");
    }

    void serialize(Luau::AstExprConstantBool* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "boolean");

        lua_pushboolean(L, node->value);
        lua_setfield(L, -2, "value");
    }

    void serialize(Luau::AstExprConstantNumber* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "number");

        lua_pushnumber(L, node->value);
        lua_setfield(L, -2, "value");
    }

    void serialize(Luau::AstExprConstantString* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "number");

        lua_pushlstring(L, node->value.data, node->value.size);
        lua_setfield(L, -2, "value");
    }

    void serialize(Luau::AstExprLocal* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "local");

        // TODO: locals
        lua_pushnil(L);
        lua_setfield(L, -2, "local");

        lua_pushboolean(L, node->upvalue);
        lua_setfield(L, -2, "upvalue");
    }

    void serialize(Luau::AstExprGlobal* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "global");

        lua_pushstring(L, node->name.value);
        lua_setfield(L, -2, "name");
    }

    void serialize(Luau::AstExprVarargs* node)
    {
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "vararg");
    }

    void serialize(Luau::AstExprCall* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "call");

        node->func->visit(this);
        lua_setfield(L, -2, "func");

        serializeExprs(node->args, 1);
        withLocation(node->argLocation);
        lua_setfield(L, -2, "arguments");
    }

    void serialize(Luau::AstExprIndexName* node)
    {
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "indexname");

        node->expr->visit(this);
        lua_setfield(L, -2, "expr");

        serialize(node->index);
        withLocation(node->indexLocation);
        lua_setfield(L, -2, "index");

        lua_createtable(L, 0, 2);
        lua_pushlstring(L, &node->op, 1);
        lua_setfield(L, -2, "value");
        serialize(node->opPosition);
        lua_setfield(L, -2, "position");
        lua_setfield(L, -2, "accessor");
    }

    void serialize(Luau::AstExprIndexExpr* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "index");

        node->expr->visit(this);
        lua_setfield(L, -2, "expr");

        node->index->visit(this);
        lua_setfield(L, -2, "index");
    }

    void serialize(Luau::AstExprFunction* node)
    {
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "function");

        // TODO: attributes

        // TODO: locals
        lua_pushnil(L);
        lua_setfield(L, -2, "self");

        // TODO: locals
        lua_createtable(L, 0, 0);
        lua_setfield(L, -2, "parameters");

        // TODO: generics, return types, etc.

        if (node->vararg)
            serialize(node->varargLocation);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "vargarg");

        node->body->visit(this);
        lua_setfield(L, -2, "body");
    }

    void serialize(Luau::AstExprTable* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "table");

        lua_createtable(L, node->items.size, 0);
        for (size_t i = 0; i < node->items.size; i++)
        {
            serialize(node->items.data[i]);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, "entries");
    }

    void serialize(Luau::AstExprUnary* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "unary");

        switch (node->op)
        {
        case Luau::AstExprUnary::Op::Not:
            lua_pushstring(L, "not");
            break;
        case Luau::AstExprUnary::Op::Minus:
            lua_pushstring(L, "-");
            break;
        case Luau::AstExprUnary::Op::Len:
            lua_pushstring(L, "#");
            break;
        }
        lua_setfield(L, -2, "operator");

        node->expr->visit(this);
        lua_setfield(L, -2, "operand");
    }

    void serialize(Luau::AstExprBinary* node)
    {
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "binary");

        serialize(node->op);
        lua_setfield(L, -2, "operator");

        node->left->visit(this);
        lua_setfield(L, -2, "lhsoperand");

        node->right->visit(this);
        lua_setfield(L, -2, "rhsoperand");
    }

    void serialize(Luau::AstExprTypeAssertion* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "cast");

        node->expr->visit(this);
        lua_setfield(L, -2, "operand");

        lua_pushnil(L); // TODO: types
        lua_setfield(L, -2, "annotation");
    }

    void serialize(Luau::AstExprIfElse* node)
    {
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "conditional");

        node->condition->visit(this);
        lua_setfield(L, -2, "condition");

        if (node->hasThen)
            node->trueExpr->visit(this);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "consequent");

        if (node->hasElse)
            node->falseExpr->visit(this);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "antecedent");
    }

    void serialize(Luau::AstExprInterpString* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "interpolatedstring");

        lua_createtable(L, node->strings.size, 0);
        for (size_t i = 0; i < node->strings.size; i++)
        {
            lua_pushlstring(L, node->strings.data[i].data, node->strings.data[i].size);
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, -2, "strings");

        serializeExprs(node->expressions);
        lua_setfield(L, -2, "expressions");
    }

    void serialize(Luau::AstExprError* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "error");

        serializeExprs(node->expressions);
        lua_setfield(L, -2, "expressions");

        // TODO: messageIndex reference
    }

    void serializeStat(Luau::AstStatBlock* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "block");

        serializeStats(node->body);
        lua_setfield(L, -2, "statements");
    }

    void serializeStat(Luau::AstStatIf* node)
    {
        lua_createtable(L, 0, preambleSize + 5);

        serializeNodePreamble(node, "conditional");

        node->condition->visit(this);
        lua_setfield(L, -2, "condition");

        node->thenbody->visit(this);
        lua_setfield(L, -2, "consequent");

        node->elsebody->visit(this);
        lua_setfield(L, -2, "antecedent");

        if (node->thenLocation)
            serialize(*node->thenLocation);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "thenLocation");

        if (node->elseLocation)
            serialize(*node->elseLocation);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "elseLocation");
    }

    void serializeStat(Luau::AstStatWhile* node)
    {
        lua_createtable(L, 0, preambleSize + 4);

        serializeNodePreamble(node, "while");

        node->condition->visit(this);
        lua_setfield(L, -2, "condition");

        node->body->visit(this);
        lua_setfield(L, -2, "body");

        if (node->hasDo)
            serialize(node->doLocation);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "doLocation");
    }

    void serializeStat(Luau::AstStatRepeat* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "repeat");

        node->condition->visit(this);
        lua_setfield(L, -2, "condition");

        node->body->visit(this);
        lua_setfield(L, -2, "body");
    }

    void serializeStat(Luau::AstStatBreak* node)
    {
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "break");
    }

    void serializeStat(Luau::AstStatContinue* node)
    {
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "continue");
    }

    void serializeStat(Luau::AstStatReturn* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "return");

        serializeExprs(node->list);
        lua_setfield(L, -2, "expressions");
    }

    void serializeStat(Luau::AstStatExpr* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "expression");

        node->expr->visit(this);
        lua_setfield(L, -2, "expression");
    }

    void serializeStat(Luau::AstStatLocal* node)
    {
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "local");

        // TODO: locals
        lua_pushnil(L);
        lua_setfield(L, -2, "variables");

        serializeExprs(node->values);
        lua_setfield(L, -2, "values");

        if (node->equalsSignLocation)
            serialize(*node->equalsSignLocation);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "equalsSignLocation");
    }

    void serializeStat(Luau::AstStatFor* node)
    {
        lua_createtable(L, 0, preambleSize + 6);

        serializeNodePreamble(node, "for");

        // TODO: locals
        lua_pushnil(L);
        lua_setfield(L, -2, "variable");

        node->from->visit(this);
        lua_setfield(L, -2, "from");

        node->to->visit(this);
        lua_setfield(L, -2, "to");

        node->step->visit(this);
        lua_setfield(L, -2, "step");

        node->body->visit(this);
        lua_setfield(L, -2, "body");

        if (node->hasDo)
            serialize(node->doLocation);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "doLocation");
    }

    void serializeStat(Luau::AstStatForIn* node)
    {
        lua_createtable(L, 0, preambleSize + 5);

        serializeNodePreamble(node, "forin");

        // TODO: locals
        lua_pushnil(L);
        lua_setfield(L, -2, "variables");

        serializeExprs(node->values);
        lua_setfield(L, -2, "values");

        node->body->visit(this);
        lua_setfield(L, -2, "body");

        if (node->hasIn)
            serialize(node->inLocation);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "inLocation");

        if (node->hasDo)
            serialize(node->doLocation);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "doLocation");
    }

    void serializeStat(Luau::AstStatAssign* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "assign");

        serializeExprs(node->vars);
        lua_setfield(L, -2, "variables");

        serializeExprs(node->values);
        lua_setfield(L, -2, "values");
    }

    void serializeStat(Luau::AstStatCompoundAssign* node)
    {
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "compoundassign");

        serialize(node->op);
        lua_setfield(L, -2, "operand");

        node->var->visit(this);
        lua_setfield(L, -2, "variable");

        node->value->visit(this);
        lua_setfield(L, -2, "value");
    }

    void serializeStat(Luau::AstStatFunction* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "function");

        node->name->visit(this);
        lua_setfield(L, -2, "name");

        node->func->visit(this);
        lua_setfield(L, -2, "function");
    }

    void serializeStat(Luau::AstStatLocalFunction* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "localfunction");

        // TODO: locals
        lua_pushnil(L);
        lua_setfield(L, -2, "name");

        node->func->visit(this);
        lua_setfield(L, -2, "function");
    }

    void serializeStat(Luau::AstStatTypeAlias* node)
    {
        // TODO: types
    }

    void serializeStat(Luau::AstStatDeclareFunction* node)
    {
        // TODO: declarations
    }

    void serializeStat(Luau::AstStatDeclareGlobal* node)
    {
        // TODO: declarations
    }

    void serializeStat(Luau::AstStatDeclareClass* node)
    {
        // TODO: declarations
    }

    void serializeStat(Luau::AstStatError* node)
    {
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "error");

        serializeExprs(node->expressions);
        lua_setfield(L, -2, "expressions");

        serializeStats(node->statements);
        lua_setfield(L, -2, "statements");

        // TODO: messageIndex reference
    }

    void serializeType(Luau::AstTypeReference* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypeTable* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypeFunction* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypeTypeof* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypeIntersection* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypeSingletonBool* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypeSingletonString* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypeError* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypePack* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypePackExplicit* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypePackVariadic* node)
    {
        // TODO: types
    }

    void serializeType(Luau::AstTypePackGeneric* node)
    {
        // TODO: types
    }

    bool visit(Luau::AstExpr* node) override
    {
        node->visit(this);
        return false;
    }

    bool visit(Luau::AstExprGroup* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprConstantNil* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprConstantBool* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprConstantNumber* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprConstantString* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprLocal* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprGlobal* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprVarargs* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprCall* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprIndexName* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprIndexExpr* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprFunction* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprTable* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprUnary* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprBinary* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprTypeAssertion* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprIfElse* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprInterpString* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstExprError* node) override
    {
        serialize(node);
        return false;
    }

    bool visit(Luau::AstStat* node) override
    {
        node->visit(this);
        return false;
    }

    bool visit(Luau::AstStatBlock* node) override
    {
        serializeStats(node->body);
        return false;
    }

    bool visit(Luau::AstStatIf* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatWhile* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatRepeat* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatBreak* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatContinue* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatReturn* node) override
    {
        serializeExprs(node->list);

        return true;
    }

    bool visit(Luau::AstStatExpr* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatLocal* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatFor* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatForIn* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatAssign* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatCompoundAssign* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatFunction* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatLocalFunction* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatTypeAlias* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatDeclareFunction* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatDeclareGlobal* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatDeclareClass* node) override
    {
        return true;
    }

    bool visit(Luau::AstStatError* node) override
    {
        return true;
    }

    bool visit(Luau::AstType* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypeReference* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypeTable* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypeFunction* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypeTypeof* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypeUnion* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypeIntersection* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypeSingletonBool* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypeSingletonString* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypeError* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypePack* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypePackExplicit* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypePackVariadic* node) override
    {
        return true;
    }

    bool visit(Luau::AstTypePackGeneric* node) override
    {
        return true;
    }
};

static int luau_parse(lua_State* L)
{
    std::string source = luaL_checkstring(L, 1);

    Luau::ParseResult result = parse(source);

    if (!result.errors.empty())
    {
        std::vector<std::string> locationStrings{};
        locationStrings.reserve(result.errors.size());

        size_t size = 0;
        for (auto error : result.errors)
        {
            locationStrings.emplace_back(Luau::toString(error.getLocation()));
            size += locationStrings.back().size() + 2 + error.getMessage().size() + 1;
        }

        std::string fullError;
        fullError.reserve(size);

        for (size_t i = 0; i < result.errors.size(); i++)
        {
            fullError += locationStrings[i];
            fullError += ": ";
            fullError += result.errors[i].getMessage();
            fullError += "\n";
        }

        luaL_error(L, "parsing failed:\n%s", fullError.c_str());
    }

    lua_createtable(L, 0, 2);

    AstSerialize serializer{L};
    serializer.visit(result.root);
    lua_setfield(L, -2, "root");

    lua_pushnumber(L, result.lines);
    lua_setfield(L, -2, "lines");

    return 1;
}

static int luau_parseexpr(lua_State* L)
{
    std::string source = luaL_checkstring(L, 1);

    ExprResult result = parseExpr(source);

    if (!result.errors.empty())
    {
        std::vector<std::string> locationStrings{};
        locationStrings.reserve(result.errors.size());

        size_t size = 0;
        for (auto error : result.errors)
        {
            locationStrings.emplace_back(Luau::toString(error.getLocation()));
            size += locationStrings.back().size() + 2 + error.getMessage().size() + 1;
        }

        std::string fullError;
        fullError.reserve(size);

        for (size_t i = 0; i < result.errors.size(); i++)
        {
            fullError += locationStrings[i];
            fullError += ": ";
            fullError += result.errors[i].getMessage();
            fullError += "\n";
        }

        luaL_error(L, "parsing failed:\n%s", fullError.c_str());
    }

    AstSerialize serializer{L};
    serializer.visit(result.root);

    return 1;
}

static const luaL_Reg lib[] = {
    {"parse", luau_parse},
    {"parseexpr", luau_parseexpr},
    {nullptr, nullptr},
};

} // namespace luau

int luaopen_luau(lua_State* L)
{
    luaL_register(L, "luau", luau::lib);

    return 1;
}
