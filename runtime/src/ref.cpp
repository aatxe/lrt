#include "queijo/ref.h"
#include "queijo/runtime.h"

#include "lua.h"

#include <assert.h>

Ref::Ref(lua_State* L, int idx)
{
    GL = lua_mainthread(L);
    refId = lua_ref(L, idx);
}

Ref::~Ref()
{
    [[maybe_unused]] Runtime* runtime = getRuntime(GL);
    assert(!runtime || runtime->runtimeThread == uv_thread_self());

    lua_unref(GL, refId);
}

void Ref::push(lua_State* L) const
{
    lua_getref(L, refId);
}

std::shared_ptr<Ref> getRefForThread(lua_State* L)
{
    lua_pushthread(L);
    std::shared_ptr<Ref> ref = std::make_shared<Ref>(L, -1);
    lua_pop(L, 1);
    return ref;
}
