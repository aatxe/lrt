#pragma once

#include "lua.h"

#include <memory>
#include <utility>

// Only interact with Ref from main thread! (one day it might even get enforced)
struct Ref
{
    Ref(lua_State* L, int idx)
    {
        GL = lua_mainthread(L);
        refId = lua_ref(L, idx);
    }
    ~Ref()
    {
        lua_unref(GL, refId);
    }
    Ref(Ref&& rhs) noexcept = delete;
    Ref& operator=(Ref&& rhs) noexcept = delete;
    Ref(const Ref& rhs) = delete;
    Ref& operator=(const Ref& rhs) = delete;

    void push(lua_State* L) const
    {
        lua_getref(L, refId);
    }

    lua_State* GL = nullptr;
    int refId = 0;
};

inline std::shared_ptr<Ref> getRefForThread(lua_State* L)
{
    lua_pushthread(L);
    std::shared_ptr<Ref> ref = std::make_shared<Ref>(L, -1);
    lua_pop(L, 1);
    return ref;
}
