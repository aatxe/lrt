#pragma once

struct lua_State;

int lua_require(lua_State* L);
int lua_requireFromSource(lua_State* L);
