#include "Luau/Common.h"
#include "lua.h"
#include "lualib.h"

#include "Luau/CodeGen.h"
#include "Luau/Compiler.h"
#include "Luau/FileUtils.h"
#include "Luau/Parser.h"
#include "Luau/Require.h"

#include "queijo/net.h"
#include "queijo/fs.h"
#include "queijo/ref.h"
#include "queijo/runtime.h"
#include "uv.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <mutex>
#include <string>
#include <vector>

static bool codegen = false;
static int program_argc = 0;
static char** program_argv = nullptr;

static Runtime runtime;

static Luau::CompileOptions copts()
{
    Luau::CompileOptions result = {};
    result.optimizationLevel = 2;
    result.debugLevel = 2;
    result.typeInfoLevel = 1;
    result.coverageLevel = 0;

    return result;
}

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

static int lua_require(lua_State* L)
{
    std::string name = luaL_checkstring(L, 1);

    RequireResolver::ResolvedRequire resolvedRequire;
    {
        lua_Debug ar;
        lua_getinfo(L, 1, "s", &ar);

        RuntimeRequireContext requireContext{ar.source};
        RuntimeCacheManager cacheManager{L};
        RuntimeErrorHandler errorHandler{L};

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
        if (codegen)
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

void setupState(Runtime& runtime, lua_State* L)
{
    runtime.GL = L;

    lua_setthreaddata(L, &runtime);

    /* register new libraries */
    if (Luau::CodeGen::isSupported())
        Luau::CodeGen::create(L);

    // register the builtin tables
    luaL_openlibs(L);

    luaopen_net(L);
    luaopen_fs(L);

    lua_pop(L, 1);

    static const luaL_Reg funcs[] = {
        {"require", lua_require},
        {nullptr, nullptr},
    };

    luaL_register(L, "_G", funcs);

    luaL_sandbox(L);
}

bool setupArguments(lua_State* L, int argc, char** argv)
{
    if (!lua_checkstack(L, argc))
        return false;

    for (int i = 0; i < argc; ++i)
        lua_pushstring(L, argv[i]);

    return true;
}

static bool runToCompletion(Runtime& runtime)
{
    // While there is some C++ or Luau code left to run
    while (!runtime.runningThreads.empty() || runtime.hasContinuations())
    {
        uv_run(uv_default_loop(), UV_RUN_DEFAULT);
        // Complete all C++ continuations
        std::vector<std::function<void()>> continuations;

        {
            std::unique_lock lock(runtime.continuationMutex);
            continuations = std::move(runtime.continuations);
            runtime.continuations.clear();
        }

        for (auto&& continuation : continuations)
            continuation();

        if (runtime.runningThreads.empty())
            continue;

        auto next = std::move(runtime.runningThreads.front());
        runtime.runningThreads.erase(runtime.runningThreads.begin());

        next.ref->push(runtime.GL);
        lua_State* L = lua_tothread(runtime.GL, -1);

        if (L == nullptr)
        {
            fprintf(stderr, "Cannot resume a non-thread reference");
            return false;
        }

        // We still have 'next' on stack to hold on to thread we are about to run
        lua_pop(runtime.GL, 1);

        int status = LUA_OK;

        if (!next.success)
            status = lua_resumeerror(L, nullptr);
        else
            status = lua_resume(L, nullptr, next.argumentCount);

        if (status == LUA_YIELD)
        {
            int results = lua_gettop(L);

            if (results != 0)
            {
                std::string error = "Top level yield cannot return any results";
                error += "\nstacktrace:\n";
                error += lua_debugtrace(L);
                fprintf(stderr, "%s", error.c_str());
                return false;
            }

            runtime.runningThreads.push_back({true, getRefForThread(L), 0});
            continue;
        }

        if (status != LUA_OK)
        {
            std::string error;

            if (const char* str = lua_tostring(L, -1))
                error = str;

            error += "\nstacktrace:\n";
            error += lua_debugtrace(L);

            fprintf(stderr, "%s", error.c_str());
            return false;
        }
    }

    return true;
}

static bool runFile(Runtime& runtime, const char* name, lua_State* GL)
{
    std::optional<std::string> source = readFile(name);
    if (!source)
    {
        fprintf(stderr, "Error opening %s\n", name);
        return false;
    }

    // module needs to run in a new thread, isolated from the rest
    lua_State* L = lua_newthread(GL);

    // new thread needs to have the globals sandboxed
    luaL_sandboxthread(L);

    std::string chunkname = "=" + std::string(name);

    std::string bytecode = Luau::compile(*source, copts());

    if (luau_load(L, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) != 0)
    {
        if (const char* str = lua_tostring(L, -1))
            fprintf(stderr, "%s", str);
        else
            fprintf(stderr, "Failed to load bytecode");

        lua_pop(GL, 1);
        return false;
    }

    if (codegen)
    {
        Luau::CodeGen::CompilationOptions nativeOptions;
        Luau::CodeGen::compile(L, -1, nativeOptions);
    }

    if (!setupArguments(L, program_argc, program_argv))
    {
        fprintf(stderr, "Failed to pass arguments to Luau");
        lua_pop(GL, 1);
        return false;
    }

    runtime.GL = GL;
    runtime.runningThreads.push_back({true, getRefForThread(L), program_argc});

    lua_pop(GL, 1);

    return runToCompletion(runtime);
}

static void displayHelp(const char* argv0)
{
    printf("Usage: %s [options] [file list] [--] [arg list]\n", argv0);
    printf("\n");
    printf("When file list is omitted, an interactive REPL is started instead.\n");
    printf("\n");
    printf("Available options:\n");
    printf("  -h, --help: Display this usage message.\n");
    printf("  --: declare start of arguments to be passed to the Luau program\n");
}

static int assertionHandler(const char* expr, const char* file, int line, const char* function)
{
    printf("%s(%d): ASSERTION FAILED: %s\n", file, line, expr);
    return 1;
}

int main(int argc, char** argv)
{
    Luau::assertHandler() = assertionHandler;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    int program_args = argc;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            displayHelp(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--") == 0)
        {
            program_args = i + 1;
            break;
        }
        else if (argv[i][0] == '-')
        {
            fprintf(stderr, "Error: Unrecognized option '%s'.\n\n", argv[i]);
            displayHelp(argv[0]);
            return 1;
        }
    }

    program_argc = argc - program_args;
    program_argv = &argv[program_args];

    const std::vector<std::string> files = getSourceFiles(argc, argv);

    if (files.empty())
    {
        fprintf(stderr, "Error: queijo expects a file to run.\n\n");
        displayHelp(argv[0]);
        return 1;
    }

    std::unique_ptr<lua_State, void (*)(lua_State*)> globalState(luaL_newstate(), lua_close);
    lua_State* L = globalState.get();

    Runtime runtime;

    setupState(runtime, L);

    int failed = 0;

    for (size_t i = 0; i < files.size(); ++i)
    {
        bool isLastFile = i == files.size() - 1;
        failed += !runFile(runtime, files[i].c_str(), L);
    }

    return failed ? 1 : 0;
}
