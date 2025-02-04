#pragma once

#include <memory>
#include <utility>

struct lua_State;

struct Ref
{
    Ref(lua_State* L, int idx);
    ~Ref();
    Ref(Ref&& rhs) noexcept = delete;
    Ref& operator=(Ref&& rhs) noexcept = delete;
    Ref(const Ref& rhs) = delete;
    Ref& operator=(const Ref& rhs) = delete;

    void push(lua_State* L) const;

    lua_State* GL = nullptr;
    int refId = 0;
};

std::shared_ptr<Ref> getRefForThread(lua_State* L);
