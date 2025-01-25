#include "queijo/spawn.h"

#include "queijo/require.h"
#include "queijo/runtime.h"

#include <memory>

#include "lua.h"
#include "lualib.h"

// TODO: move setup to a reachable place as well
lua_State* setupState(Runtime& runtime);

struct TargetFunction
{
    std::shared_ptr<Runtime> runtime;
    std::shared_ptr<Ref> func;
};

constexpr int kTargetFunctionTag = 1;

static bool copyLuauObject(lua_State* from, lua_State* to, int fromIdx)
{
    switch (lua_type(from, fromIdx))
    {
    case LUA_TNIL:
        lua_pushnil(to);
        break;
    case LUA_TBOOLEAN:
        lua_pushboolean(to, lua_toboolean(from, fromIdx));
        break;
    case LUA_TNUMBER:
        lua_pushnumber(to, lua_tonumber(from, fromIdx));
        break;
    case LUA_TSTRING:
    {
        size_t len = 0;
        const char* str = lua_tolstring(from, fromIdx, &len);
        lua_pushlstring(to, str, len);
    }
    break;
    case LUA_TTABLE:
        lua_createtable(to, 0, 0);

        for (int i = 0; i = lua_rawiter(from, fromIdx, i), i >= 0;)
        {
            if (!copyLuauObject(from, to, -2))
            {
                lua_pop(from, 2);
                return false;
            }

            if (!copyLuauObject(from, to, -1))
            {
                lua_pop(from, 2);
                return false;
            }

            lua_rawset(to, -3);

            lua_pop(from, 2);
        }
        break;
    default:
        return false;
    }

    return true;
}

static std::shared_ptr<Ref> packStackValues(lua_State* from, std::shared_ptr<Runtime> runtime)
{
    std::unique_lock lock(runtime->dataCopyMutex);
    lua_State* to = runtime->dataCopy.get();

    lua_createtable(to, lua_gettop(from), 0);

    for (int i = 0; i < lua_gettop(from); i++)
    {
        if (!copyLuauObject(from, to, i + 1))
            luaL_error(from, "Failed to copy arguments between VMs");

        lua_rawseti(to, -2, i + 1);
    }

    auto args = std::make_shared<Ref>(to, -1);
    lua_pop(to, 1);

    return args;
}

static int unpackStackValue(std::shared_ptr<Runtime> runtime, lua_State* to, std::shared_ptr<Ref> ref)
{
    std::unique_lock lock(runtime->dataCopyMutex);
    lua_State* from = runtime->dataCopy.get();

    ref->push(from);
    int count = lua_objlen(from, -1);

    for (int i = 0; i < count; i++)
    {
        lua_rawgeti(from, -1, i + 1);

        if (!copyLuauObject(from, to, -1))
            luaL_error(to, "Failed to copy arguments between VMs"); // TODO: might not be in a VM protected call

        lua_pop(from, 1);
    }

    lua_pop(from, 1);
    return count;
}

static int crossVmMarshall(lua_State* L)
{
    TargetFunction& target = *(TargetFunction*)lua_touserdatatagged(L, lua_upvalueindex(1), kTargetFunctionTag);

    // Copy arguments into the data copy VM
    std::shared_ptr<Ref> args = packStackValues(L, target.runtime);

    auto source = getResumeToken(L);

    target.runtime->schedule([source, target = target, args] {
        lua_State* L = lua_newthread(target.runtime->GL);
        luaL_sandboxthread(L);

        target.func->push(L);

        int argCount = unpackStackValue(target.runtime, L, args);

        auto co = getRefForThread(L);
        lua_pop(target.runtime->GL, 1);

        target.runtime->runningThreads.push_back({ true, co, argCount, [source, target = target.runtime, co] {
            co->push(target->GL);
            lua_State* L = lua_tothread(target->GL, -1);
            lua_pop(target->GL, 1);

            std::shared_ptr<Ref> rets = packStackValues(L, target);

            source->complete([target, rets](lua_State* L) {
                return unpackStackValue(target, L, rets);
            });
        }});
    });

    return lua_yield(L, 0);
}

static int crossVmMarshallCont(lua_State* L, int status)
{
    if (status == LUA_OK)
        return lua_gettop(L);

    luaL_error(L, "async function errored");
    return 0;
}

namespace vm {

int lua_spawn(lua_State* L)
{
    const char* file = luaL_checkstring(L, 1);

    auto child = std::make_shared<Runtime>();

    setupState(*child);

    lua_Debug ar;
    lua_getinfo(L, 1, "s", &ar);

    // Require the target module
    lua_pushcclosure(child->GL, lua_requireFromSource, "require", 0);
    lua_pushstring(child->GL, file);
    lua_pushstring(child->GL, ar.source);
    int status = lua_pcall(child->GL, 2, 1, 0);

    if (status == LUA_ERRRUN && lua_type(child->GL, -1) == LUA_TSTRING)
    {
        size_t len = 0;
        const char* str = lua_tolstring(child->GL, -1, &len);

        std::string error = std::string(str, len);
        error += "\nstacktrace:\n";
        error += lua_debugtrace(child->GL);
        luaL_error(L, "Failed to spawn, target module error: %s", error.c_str());
    }

    if (status != LUA_OK)
        luaL_error(L, "Failed to require %s", file);

    if (lua_type(child->GL, -1) != LUA_TTABLE)
        luaL_error(L, "Module %s did not return a table", file);

    lua_setuserdatadtor(L, kTargetFunctionTag, [](lua_State* L, void* userdata) {
        // Current runtime VM is dropping a foreign VM Ref
        // It has to be released in target runtime, so we copy it over
        TargetFunction* target = (TargetFunction*)userdata;

        // Schedule references to be removed in target runtime
        target->runtime->schedule([func = target->func]() mutable {
            func.reset();
        });

        // Remove the Ref we have in current VM, now it will not cause the actual lua_unref
        target->~TargetFunction();
    });

    // For each function in the child VM return table, create a wrapper function in main VM which will marshall a call
    lua_createtable(L, 0, 0);

    for (int i = 0; i = lua_rawiter(child->GL, -1, i), i >= 0;)
    {
        if (lua_type(child->GL, -2) != LUA_TSTRING || lua_type(child->GL, -1) != LUA_TFUNCTION)
        {
            lua_pop(child->GL, 2);
            continue;
        }

        size_t length = 0;
        const char* name = lua_tolstring(child->GL, -2, &length);

        auto func = std::make_shared<Ref>(child->GL, -1);

        TargetFunction* target = new (lua_newuserdatatagged(L, sizeof(TargetFunction), kTargetFunctionTag)) TargetFunction();

        target->runtime = child;
        target->func = func;

        lua_pushcclosurek(L, crossVmMarshall, name, 1, crossVmMarshallCont);
        lua_setfield(L, -2, name);

        lua_pop(child->GL, 2);
    }

    lua_pop(child->GL, 1);

    child->runContinuously();

    return 1;
}

} // namespace vm
