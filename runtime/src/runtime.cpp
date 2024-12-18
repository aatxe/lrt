#include "queijo/runtime.h"

#include <assert.h>

bool Runtime::hasContinuations()
{
    std::unique_lock lock(continuationMutex);
    return !continuations.empty();
}

void Runtime::scheduleLuauContinuation(std::function<bool(lua_State*)> cb)
{
    std::unique_lock lock(continuationMutex);

    continuations.push_back([this, cb] {
        bool success = cb(GL);
        lua_State* L = lua_tothread(GL, -1);
        assert(L);

        runningThreads.push_back({ success, Ref(GL, -1), lua_gettop(L) });
        lua_pop(GL, 1);
    });
}

Runtime* getRuntime(lua_State* L)
{
    return reinterpret_cast<Runtime*>(lua_getthreaddata(lua_mainthread(L)));
}
