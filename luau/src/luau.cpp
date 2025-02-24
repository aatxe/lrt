#include "lute/luau.h"

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

struct StatResult
{
    std::shared_ptr<Luau::Allocator> allocator;
    std::shared_ptr<Luau::AstNameTable> names;

    Luau::ParseResult parseResult;
};

static StatResult parse(std::string& source)
{
    auto allocator = std::make_shared<Luau::Allocator>();
    auto names = std::make_shared<Luau::AstNameTable>(*allocator);

    Luau::ParseOptions options;
    options.captureComments = true;
    options.allowDeclarationSyntax = false;

    auto parseResult = Luau::Parser::parse(source.data(), source.size(), *names, *allocator, options);

    return StatResult{allocator, names, std::move(parseResult)};
}

struct ExprResult
{
    std::shared_ptr<Luau::Allocator> allocator;
    std::shared_ptr<Luau::AstNameTable> names;

    Luau::AstExpr* root;
    size_t lines = 0;

    std::vector<Luau::HotComment> hotcomments;
    std::vector<Luau::ParseError> errors;

    std::vector<Luau::Comment> commentLocations;
};

static ExprResult parseExpr(std::string& source)
{
    auto allocator = std::make_shared<Luau::Allocator>();
    auto names = std::make_shared<Luau::AstNameTable>(*allocator);

    Luau::ParseOptions options;
    options.captureComments = true;
    options.allowDeclarationSyntax = false;

    Luau::Parser p(source.data(), source.size(), *names, *allocator, options);

    try
    {
        Luau::AstExpr* expr = p.parseExpr();
        size_t lines = p.lexer.current().location.end.line + (source.size() > 0 && source.data()[source.size() - 1] != '\n');

        return ExprResult{allocator, names, expr, lines, std::move(p.hotcomments), std::move(p.parseErrors), std::move(p.commentLocations)};
    }
    catch (Luau::ParseError& err)
    {
        // when catching a fatal error, append it to the list of non-fatal errors and return
        p.parseErrors.push_back(err);

        return ExprResult{nullptr, nullptr, nullptr, 0, {}, std::move(p.parseErrors)};
    }
}

struct AstSerialize : public Luau::AstVisitor
{
    lua_State* L;

    // absolute index for the table where we're storing locals
    int localTableIndex;

    AstSerialize(lua_State* L)
        : L(L)
    {
        lua_createtable(L, 0, 0);
        localTableIndex = lua_absindex(L, -1);
    }

    void serialize(Luau::Position position)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, 2);

        lua_pushnumber(L, position.line);
        lua_setfield(L, -2, "line");

        lua_pushnumber(L, position.column);
        lua_setfield(L, -2, "column");
    }

    void serialize(Luau::Location location)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, 2);

        serialize(location.begin);
        lua_setfield(L, -2, "begin");

        serialize(location.end);
        lua_setfield(L, -2, "end");
    }

    void serialize(Luau::AstName& name)
    {
        lua_rawcheckstack(L, 1);
        lua_pushstring(L, name.value);
    }

    void serialize(Luau::AstLocal* local)
    {
        lua_rawcheckstack(L, 2);

        lua_pushlightuserdata(L, local);
        lua_gettable(L, localTableIndex);

        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            lua_createtable(L, 0, 3);

            // set up reference for this local into the local table
            lua_pushlightuserdata(L, local);
            lua_pushvalue(L, -2);
            lua_settable(L, localTableIndex);

            serialize(local->name);
            lua_setfield(L, -2, "name");

            if (local->shadow)
                serialize(local->shadow);
            else
                lua_pushnil(L);
            lua_setfield(L, -2, "shadows");

            // TODO: types
            lua_pushnil(L);
            lua_setfield(L, -2, "annotation");
        }
    }

    void serialize(Luau::AstExprTable::Item& item)
    {
        lua_rawcheckstack(L, 2);
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
        lua_rawcheckstack(L, 2);

        lua_pushstring(L, tag);
        lua_setfield(L, -2, "tag");

        withLocation(node->location);
    }

    void serializeLocals(Luau::AstArray<Luau::AstLocal*>& locals, size_t nrec = 0)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, locals.size, nrec);

        for (size_t i = 0; i < locals.size; i++)
        {
            serialize(locals.data[i]);
            lua_rawseti(L, -2, i + 1);
        }
    }

    void serializeExprs(Luau::AstArray<Luau::AstExpr*>& exprs, size_t nrec = 0)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, exprs.size, nrec);

        for (size_t i = 0; i < exprs.size; i++)
        {
            exprs.data[i]->visit(this);
            lua_rawseti(L, -2, i + 1);
        }
    }

    void serializeStats(Luau::AstArray<Luau::AstStat*>& stats, size_t nrec = 0)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, stats.size, nrec);

        for (size_t i = 0; i < stats.size; i++)
        {
            stats.data[i]->visit(this);
            lua_rawseti(L, -2, i + 1);
        }
    }

    void serialize(Luau::AstExprGroup* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "group");

        node->expr->visit(this);
        lua_setfield(L, -2, "expression");
    }

    void serialize(Luau::AstExprConstantNil* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "nil");
    }

    void serialize(Luau::AstExprConstantBool* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "boolean");

        lua_pushboolean(L, node->value);
        lua_setfield(L, -2, "value");
    }

    void serialize(Luau::AstExprConstantNumber* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "number");

        lua_pushnumber(L, node->value);
        lua_setfield(L, -2, "value");
    }

    void serialize(Luau::AstExprConstantString* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "number");

        lua_pushlstring(L, node->value.data, node->value.size);
        lua_setfield(L, -2, "value");
    }

    void serialize(Luau::AstExprLocal* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "local");

        serialize(node->local);
        lua_setfield(L, -2, "local");

        lua_pushboolean(L, node->upvalue);
        lua_setfield(L, -2, "upvalue");
    }

    void serialize(Luau::AstExprGlobal* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "global");

        lua_pushstring(L, node->name.value);
        lua_setfield(L, -2, "name");
    }

    void serialize(Luau::AstExprVarargs* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "vararg");
    }

    void serialize(Luau::AstExprCall* node)
    {
        lua_rawcheckstack(L, 2);
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
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "indexname");

        node->expr->visit(this);
        lua_setfield(L, -2, "expr");

        serialize(node->index);
        lua_setfield(L, -2, "index");
        serialize(node->indexLocation);
        lua_setfield(L, -2, "indexLocation");

        lua_createtable(L, 0, 2);
        lua_pushlstring(L, &node->op, 1);
        lua_setfield(L, -2, "value");
        serialize(node->opPosition);
        lua_setfield(L, -2, "position");
        lua_setfield(L, -2, "accessor");
    }

    void serialize(Luau::AstExprIndexExpr* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "index");

        node->expr->visit(this);
        lua_setfield(L, -2, "expr");

        node->index->visit(this);
        lua_setfield(L, -2, "index");
    }

    void serialize(Luau::AstExprFunction* node)
    {
        lua_rawcheckstack(L, 3);
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "function");

        // TODO: attributes

        if (node->self)
            serialize(node->self);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "self");

        serializeLocals(node->args, node->argLocation ? 1 : 0);
        if (node->argLocation)
            withLocation(*node->argLocation);
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
        lua_rawcheckstack(L, 3);
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
        lua_rawcheckstack(L, 2);
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
        lua_rawcheckstack(L, 2);
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
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "cast");

        node->expr->visit(this);
        lua_setfield(L, -2, "operand");

        lua_pushnil(L); // TODO: types
        lua_setfield(L, -2, "annotation");
    }

    void serialize(Luau::AstExprIfElse* node)
    {
        lua_rawcheckstack(L, 2);
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
        lua_rawcheckstack(L, 3);
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
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "error");

        serializeExprs(node->expressions);
        lua_setfield(L, -2, "expressions");

        // TODO: messageIndex reference
    }

    void serializeStat(Luau::AstStatBlock* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "block");

        serializeStats(node->body);
        lua_setfield(L, -2, "statements");
    }

    void serializeStat(Luau::AstStatIf* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 5);

        serializeNodePreamble(node, "conditional");

        node->condition->visit(this);
        lua_setfield(L, -2, "condition");

        node->thenbody->visit(this);
        lua_setfield(L, -2, "consequent");

        if (node->elsebody)
            node->elsebody->visit(this);
        else
            lua_pushnil(L);
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
        lua_rawcheckstack(L, 2);
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
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "repeat");

        node->condition->visit(this);
        lua_setfield(L, -2, "condition");

        node->body->visit(this);
        lua_setfield(L, -2, "body");
    }

    void serializeStat(Luau::AstStatBreak* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "break");
    }

    void serializeStat(Luau::AstStatContinue* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "continue");
    }

    void serializeStat(Luau::AstStatReturn* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "return");

        serializeExprs(node->list);
        lua_setfield(L, -2, "expressions");
    }

    void serializeStat(Luau::AstStatExpr* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "expression");

        node->expr->visit(this);
        lua_setfield(L, -2, "expression");
    }

    void serializeStat(Luau::AstStatLocal* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "local");

        serializeLocals(node->vars);
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
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 6);

        serializeNodePreamble(node, "for");

        serialize(node->var);
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
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 5);

        serializeNodePreamble(node, "forin");

        serializeLocals(node->vars);
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
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "assign");

        serializeExprs(node->vars);
        lua_setfield(L, -2, "variables");

        serializeExprs(node->values);
        lua_setfield(L, -2, "values");
    }

    void serializeStat(Luau::AstStatCompoundAssign* node)
    {
        lua_rawcheckstack(L, 2);
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
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "function");

        node->name->visit(this);
        lua_setfield(L, -2, "name");

        node->func->visit(this);
        lua_setfield(L, -2, "function");
    }

    void serializeStat(Luau::AstStatLocalFunction* node)
    {
        lua_rawcheckstack(L, 2);
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "localfunction");

        serialize(node->name);
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
        lua_rawcheckstack(L, 2);
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
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatIf* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatWhile* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatRepeat* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatBreak* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatContinue* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatReturn* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatExpr* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatLocal* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatFor* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatForIn* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatAssign* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatCompoundAssign* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatFunction* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatLocalFunction* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatTypeAlias* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatDeclareFunction* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatDeclareGlobal* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatDeclareClass* node) override
    {
        serializeStat(node);
        return false;
    }

    bool visit(Luau::AstStatError* node) override
    {
        serializeStat(node);
        return false;
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

int luau_parse(lua_State* L)
{
    std::string source = luaL_checkstring(L, 1);

    StatResult result = parse(source);

    auto& errors = result.parseResult.errors;

    if (!errors.empty())
    {
        std::vector<std::string> locationStrings{};
        locationStrings.reserve(errors.size());

        size_t size = 0;
        for (auto error : errors)
        {
            locationStrings.emplace_back(Luau::toString(error.getLocation()));
            size += locationStrings.back().size() + 2 + error.getMessage().size() + 1;
        }

        std::string fullError;
        fullError.reserve(size);

        for (size_t i = 0; i < errors.size(); i++)
        {
            fullError += locationStrings[i];
            fullError += ": ";
            fullError += errors[i].getMessage();
            fullError += "\n";
        }

        luaL_error(L, "parsing failed:\n%s", fullError.c_str());
    }

    lua_rawcheckstack(L, 3);

    lua_createtable(L, 0, 2);

    AstSerialize serializer{L};
    serializer.visit(result.parseResult.root);
    lua_setfield(L, -2, "root");

    lua_pushnumber(L, result.parseResult.lines);
    lua_setfield(L, -2, "lines");

    return 1;
}

int luau_parseexpr(lua_State* L)
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

} // namespace luau

int luaopen_luau(lua_State* L)
{
    luaL_register(L, "luau", luau::lib);

    return 1;
}

int luteopen_luau(lua_State* L)
{
    lua_createtable(L, 0, std::size(luau::lib));

    for (auto& [name, func] : luau::lib)
    {
        if (!name || !func)
            break;

        lua_pushcfunction(L, func, name);
        lua_setfield(L, -2, name);
    }

    lua_setreadonly(L, -1, 1);

    return 1;
}
