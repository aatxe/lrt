#pragma once

#include "lua.h"
#include "lualib.h"

#include "queijo/spawn.h"

// open the library as a standard global luau library
int luaopen_vm(lua_State* L);
// open the library as a table on top of the stack
int lrtopen_vm(lua_State* L);

namespace vm
{

int lua_defer(lua_State* L);

static const luaL_Reg lib[] = {
    {"create", lua_spawn},
    {nullptr, nullptr},
};

} // namespace vm
