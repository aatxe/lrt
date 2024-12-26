#pragma once

#include "lua.h"
#include "lualib.h"

int luainit_net(lua_State* L);
int luaopen_net(lua_State* L);

namespace net
{

int get(lua_State* L);

int getAsync(lua_State* L);

static const luaL_Reg lib[] = {
    {"get", get},
    {"getAsync", getAsync},
    {nullptr, nullptr},
};

}
