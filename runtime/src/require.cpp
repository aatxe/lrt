#include "lute/require.h"

#include "lute/options.h"

#include "lua.h"
#include "lualib.h"

#include "Luau/Compiler.h"
#include "Luau/CodeGen.h"
#include "Luau/Require.h"
#include "Luau/StringUtils.h"

static int finishrequire(lua_State* L)
{
    if (lua_isstring(L, -1))
        lua_error(L);

    return 1;
}

struct RuntimeRequireContext : public RequireResolver::RequireContext
{
    // In the context of the REPL, source is the calling context's chunkname.
    //
    // These chunknames have certain prefixes that indicate context. These
    // are used when displaying debug information (see luaO_chunkid).
    //
    // Generally, the '@' prefix is used for filepaths, and the '=' prefix is
    // used for custom chunknames, such as =stdin.
    explicit RuntimeRequireContext(std::string source)
        : source(std::move(source))
    {
    }

    std::string getPath() override
    {
        return source.substr(1);
    }

    bool isRequireAllowed() override
    {
        return true;
    }

    bool isStdin() override
    {
        return source == "=stdin";
    }

    std::string createNewIdentifer(const std::string& path) override
    {
        return "@" + path;
    }

private:
    std::string source;
};

struct RuntimeCacheManager : public RequireResolver::CacheManager
{
    explicit RuntimeCacheManager(lua_State* L)
        : L(L)
    {
    }

    bool isCached(const std::string& path) override
    {
        luaL_findtable(L, LUA_REGISTRYINDEX, "_MODULES", 1);
        lua_getfield(L, -1, path.c_str());
        bool cached = !lua_isnil(L, -1);
        lua_pop(L, 2);

        if (cached)
            cacheKey = path;

        return cached;
    }

    std::string cacheKey;

private:
    lua_State* L;
};

struct RuntimeErrorHandler : RequireResolver::ErrorHandler
{
    explicit RuntimeErrorHandler(lua_State* L)
        : L(L)
    {
    }

    void reportError(const std::string message) override
    {
        luaL_errorL(L, "%s", message.c_str());
    }

private:
    lua_State* L;
};

static int lua_requireInternal(lua_State* L, std::string name, std::string context)
{
    RequireResolver::ResolvedRequire resolvedRequire;

    if (name.rfind("@lute/", 0) == 0)
    {
        resolvedRequire.identifier = name;
        resolvedRequire.absolutePath = name;

        lua_getfield(L, LUA_REGISTRYINDEX, "_MODULES");
        lua_getfield(L, -1, name.c_str());

        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            lua_pushfstringL(L, "no luau runtime library: %s", name.c_str());
            resolvedRequire.status = RequireResolver::ModuleStatus::ErrorReported;
            resolvedRequire.sourceCode = Luau::format("return error('no luau runtime library: %s')", name.c_str());
        }
        else
        {
            resolvedRequire.status = RequireResolver::ModuleStatus::Cached;
            // FIXME: we could probably map this to a definition file at least
            resolvedRequire.sourceCode = "";
        }

        return finishrequire(L);
    }
    else
    {
        RuntimeRequireContext requireContext{ context };
        RuntimeCacheManager cacheManager{ L };
        RuntimeErrorHandler errorHandler{ L };

        RequireResolver resolver(std::move(name), requireContext, cacheManager, errorHandler);

        resolvedRequire = resolver.resolveRequire(
            [L, &cacheKey = cacheManager.cacheKey](const RequireResolver::ModuleStatus status)
            {
                lua_getfield(L, LUA_REGISTRYINDEX, "_MODULES");
                if (status == RequireResolver::ModuleStatus::Cached)
                    lua_getfield(L, -1, cacheKey.c_str());
            }
        );
    }

    if (resolvedRequire.status == RequireResolver::ModuleStatus::Cached)
        return finishrequire(L);

    // module needs to run in a new thread, isolated from the rest
    // note: we create ML on main thread so that it doesn't inherit environment of L
    lua_State* GL = lua_mainthread(L);
    lua_State* ML = lua_newthread(GL);
    lua_xmove(GL, L, 1);

    // new thread needs to have the globals sandboxed
    luaL_sandboxthread(ML);

    // now we can compile & run module on the new thread
    std::string bytecode = Luau::compile(resolvedRequire.sourceCode, copts());
    if (luau_load(ML, resolvedRequire.identifier.c_str(), bytecode.data(), bytecode.size(), 0) == 0)
    {
        if (getCodegenEnabled())
        {
            Luau::CodeGen::CompilationOptions nativeOptions;
            Luau::CodeGen::compile(ML, -1, nativeOptions);
        }

        int status = lua_resume(ML, L, 0);

        if (status == 0)
        {
            if (lua_gettop(ML) == 0)
                lua_pushstring(ML, "module must return a value");
            else if (!lua_istable(ML, -1) && !lua_isfunction(ML, -1))
                lua_pushstring(ML, "module must return a table or function");
        }
        else if (status == LUA_YIELD)
        {
            lua_pushstring(ML, "module can not yield");
        }
        else if (!lua_isstring(ML, -1))
        {
            lua_pushstring(ML, "unknown error while running module");
        }
    }

    // there's now a return value on top of ML; L stack: _MODULES ML
    lua_xmove(ML, L, 1);
    lua_pushvalue(L, -1);
    lua_setfield(L, -4, resolvedRequire.absolutePath.c_str());

    // L stack: _MODULES ML result
    return finishrequire(L);
}

int lua_require(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);

    lua_Debug ar;
    lua_getinfo(L, 1, "s", &ar);

    return lua_requireInternal(L, name, ar.source);
}

int lua_requireFromSource(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);
    std::string source = luaL_checkstring(L, 2);

    return lua_requireInternal(L, name, source);
}
