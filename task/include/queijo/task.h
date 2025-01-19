#pragma once

#include "lua.h"
#include "lualib.h"

#include "queijo/spawn.h"

// open the library as a standard global luau library
int luaopen_task(lua_State* L);
// open the library as a table on top of the stack
int lrtopen_task(lua_State* L);

namespace task
{

int lua_defer(lua_State* L);

static const luaL_Reg lib[] = {
    {"spawn", lua_spawn},
    {"defer", lua_defer},
    {nullptr, nullptr},
};

} // namespace task
