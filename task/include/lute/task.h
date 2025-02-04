#pragma once

#include "lua.h"
#include "lualib.h"

// open the library as a standard global luau library
int luaopen_task(lua_State* L);
// open the library as a table on top of the stack
int luteopen_task(lua_State* L);

namespace task
{

int lua_defer(lua_State* L);

static const luaL_Reg lib[] = {
    {"defer", lua_defer},
    {nullptr, nullptr},
};

} // namespace task
