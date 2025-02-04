#include "lute/task.h"

#include "lute/runtime.h"

namespace task
{
    int lua_defer(lua_State* L)
    {
        Runtime* runtime = getRuntime(L);

        runtime->runningThreads.push_back({ true, getRefForThread(L), 0 });
        return lua_yield(L, 0);
    }

} // namespace task

int luaopen_task(lua_State* L)
{
    luaL_register(L, "task", task::lib);

    return 1;
}

int luteopen_task(lua_State* L)
{
    lua_createtable(L, 0, std::size(task::lib));

    for (auto& [name, func] : task::lib)
    {
        if (!name || !func)
            break;

        lua_pushcfunction(L, func, name);
        lua_setfield(L, -2, name);
    }

    return 1;
}
