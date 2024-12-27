#pragma once

#include "lua.h"
#include "lualib.h"

// open the library as a standard global luau library
int luaopen_luau(lua_State* L);
// open the library as a table on top of the stack
int lrtopen_luau(lua_State* L);

namespace luau {

int luau_parse(lua_State* L);

int luau_parseexpr(lua_State* L);

static const luaL_Reg lib[] = {
    {"parse", luau_parse},
    {"parseexpr", luau_parseexpr},
    {nullptr, nullptr},
};

} // namespace luau
