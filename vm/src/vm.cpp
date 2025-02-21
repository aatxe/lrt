#include "lute/vm.h"

#include "lute/runtime.h"

int luaopen_vm(lua_State* L)
{
    luaL_register(L, "vm", vm::lib);

    return 1;
}

int luteopen_vm(lua_State* L)
{
    lua_createtable(L, 0, std::size(vm::lib));

    for (auto& [name, func] : vm::lib)
    {
        if (!name || !func)
            break;

        lua_pushcfunction(L, func, name);
        lua_setfield(L, -2, name);
    }

    lua_setreadonly(L, -1, 1);

    return 1;
}
