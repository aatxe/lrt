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

    if (parseResult.errors.size() > 0)
    {
        fprintf(stderr, "Parse errors were encountered:\n");
        for (const Luau::ParseError& error : parseResult.errors)
        {
            fprintf(stderr, "  %s - %s\n", Luau::toString(error.getLocation()).c_str(), error.getMessage().c_str());
        }
        fprintf(stderr, "\n");
    }

    return parseResult;
}

static Luau::AstExpr* parseExpr(std::string& source)
{
    Luau::Allocator allocator;
    Luau::AstNameTable names{allocator};

    Luau::ParseOptions options;
    options.captureComments = true;
    options.allowDeclarationSyntax = false;

    Luau::Parser p(source.data(), source.size(), names, allocator, options);

    try
    {
        return p.parseExpr();
    }
    catch (Luau::ParseError& error)
    {
        fprintf(stderr, "  %s - %s\n", Luau::toString(error.getLocation()).c_str(), error.getMessage().c_str());

        return nullptr;
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
        lua_createtable(L, 0, 2);

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
            visit(exprs.data[i]);
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

    bool serialize(Luau::AstExpr* node)
    {
        node->visit(this);
        return false;
    }

    bool serialize(Luau::AstExprGroup* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "group");

        serialize(node->expr);
        lua_setfield(L, -2, "expr");

        return true;
    }

    bool serialize(Luau::AstExprConstantNil* node)
    {
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "nil");

        return true;
    }

    bool serialize(Luau::AstExprConstantBool* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "boolean");

        lua_pushboolean(L, node->value);
        lua_setfield(L, -2, "value");

        return true;
    }

    bool serialize(Luau::AstExprConstantNumber* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "number");

        lua_pushnumber(L, node->value);
        lua_setfield(L, -2, "value");

        return true;
    }

    bool serialize(Luau::AstExprConstantString* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "number");

        lua_pushlstring(L, node->value.data, node->value.size);
        lua_setfield(L, -2, "value");

        return true;
    }

    bool serialize(Luau::AstExprLocal* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "local");

        // TODO: locals
        lua_pushnil(L);
        lua_setfield(L, -2, "local");

        lua_pushboolean(L, node->upvalue);
        lua_setfield(L, -2, "upvalue");

        return true;
    }

    bool serialize(Luau::AstExprGlobal* node)
    {
        lua_createtable(L, 0, preambleSize + 1);

        serializeNodePreamble(node, "global");

        lua_pushstring(L, node->name.value);
        lua_setfield(L, -2, "name");

        return true;
    }

    bool serialize(Luau::AstExprVarargs* node)
    {
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "vararg");

        return true;
    }

    bool serialize(Luau::AstExprCall* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "call");

        serialize(node->func);
        lua_setfield(L, -2, "func");

        serializeExprs(node->args, 1);
        withLocation(node->argLocation);
        lua_setfield(L, -2, "arguments");

        return true;
    }

    bool serialize(Luau::AstExprIndexName* node)
    {
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "indexname");

        serialize(node->expr);
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

        return true;
    }

    bool serialize(Luau::AstExprIndexExpr* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "index");

        serialize(node->expr);
        lua_setfield(L, -2, "expr");

        serialize(node->index);
        lua_setfield(L, -2, "index");

        return true;
    }

    bool serialize(Luau::AstExprFunction* node)
    {
        lua_createtable(L, 0, preambleSize);

        serializeNodePreamble(node, "function");

        // TODO: functions

        return true;
    }

    bool serialize(Luau::AstExprTable* node)
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

        return true;
    }

    bool serialize(Luau::AstExprUnary* node)
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

        serialize(node->expr);
        lua_setfield(L, -2, "operand");

        return true;
    }

    bool serialize(Luau::AstExprBinary* node)
    {
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "binary");

        switch (node->op)
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
        lua_setfield(L, -2, "operator");

        serialize(node->left);
        lua_setfield(L, -2, "leftoperand");

        serialize(node->right);
        lua_setfield(L, -2, "rightoperand");

        return true;
    }

    bool serialize(Luau::AstExprTypeAssertion* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "cast");

        serialize(node->expr);
        lua_setfield(L, -2, "operand");

        lua_pushnil(L); // TODO: types
        lua_setfield(L, -2, "annotation");

        return true;
    }

    bool serialize(Luau::AstExprIfElse* node)
    {
        lua_createtable(L, 0, preambleSize + 3);

        serializeNodePreamble(node, "conditional");

        serialize(node->condition);
        lua_setfield(L, -2, "condition");

        if (node->hasThen)
            serialize(node->trueExpr);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "consequent");

        if (node->hasElse)
            serialize(node->falseExpr);
        else
            lua_pushnil(L);
        lua_setfield(L, -2, "antecedent");

        return true;
    }

    bool serialize(Luau::AstExprInterpString* node)
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

        return true;
    }

    bool serialize(Luau::AstExprError* node)
    {
        lua_createtable(L, 0, preambleSize + 2);

        serializeNodePreamble(node, "error");

        serializeExprs(node->expressions);
        lua_setfield(L, -2, "expressions");

        // TODO: messageIndex reference

        return true;
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

    Luau::AstExpr* result = parseExpr(source);

    if (!result)
        luaL_error(L, "aaaaaaaa");

    AstSerialize serializer{L};
    serializer.visit(result);

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
