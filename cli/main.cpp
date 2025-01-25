#include "Luau/Common.h"
#include "Luau/CodeGen.h"
#include "Luau/Compiler.h"
#include "Luau/FileUtils.h"
#include "Luau/Parser.h"

#include "lua.h"
#include "lualib.h"

#include "queijo/fs.h"
#include "queijo/net.h"
#include "queijo/luau.h"
#include "queijo/ref.h"
#include "queijo/runtime.h"

#include "options.h"
#include "require.h"
#include "spawn.h"
#include "tc.h"

#include "uv.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <string>
#include <vector>

static int program_argc = 0;
static char** program_argv = nullptr;

static int lua_defer(lua_State* L)
{
    auto runtime = getRuntime(L);

    runtime->runningThreads.push_back({ true, getRefForThread(L), 0 });
    return lua_yield(L, 0);
}

lua_State* setupState(Runtime& runtime)
{
    // Separate VM for data copies
    runtime.dataCopy.reset(luaL_newstate());

    runtime.globalState.reset(luaL_newstate());

    lua_State* L = runtime.globalState.get();

    runtime.GL = L;

    lua_setthreaddata(L, &runtime);

    /* register new libraries */
    if (Luau::CodeGen::isSupported())
        Luau::CodeGen::create(L);

    // register the builtin tables
    luaL_openlibs(L);

    luaL_findtable(L, LUA_REGISTRYINDEX, "_MODULES", 1);

    lrtopen_fs(L);
    lua_setfield(L, -2, "@lrt/fs");

    lrtopen_net(L);
    lua_setfield(L, -2, "@lrt/net");

    lrtopen_luau(L);
    lua_setfield(L, -2, "@lrt/luau");

    static const luaL_Reg funcs[] = {
        {"require", lua_require},
        {"spawn", lua_spawn},
        {"defer", lua_defer},
        {nullptr, nullptr},
    };

    luaL_register(L, "_G", funcs);
    lua_pop(L, 1);

    luaL_sandbox(L);

    return L;
}

bool setupArguments(lua_State* L, int argc, char** argv)
{
    if (!lua_checkstack(L, argc))
        return false;

    for (int i = 0; i < argc; ++i)
        lua_pushstring(L, argv[i]);

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

    if (getCodegenEnabled())
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

    return runtime.runToCompletion();
}

static void displayHelp(const char* argv0)
{
    printf("Usage: %s [options] [file list] [--] [arg list]\n", argv0);
    printf("\n");
    printf("When file list is omitted, an interactive REPL is started instead.\n");
    printf("\n");
    printf("Available options:\n");
    printf("  -h, --help: Display this usage message.\n");
    printf("  --check: Run a strict typecheck of the Luau program.\n");
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
    bool runTypecheck = false;

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
        else if (strcmp(argv[i], "--check") == 0)
        {
            runTypecheck = true;
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

    // Given the source files, perform a typecheck here
    if (runTypecheck)
    {
        return typecheck(files);
    }

    if (files.empty())
    {
        fprintf(stderr, "Error: queijo expects a file to run.\n\n");
        displayHelp(argv[0]);
        return 1;
    }

    Runtime runtime;

    lua_State* L = setupState(runtime);

    int failed = 0;

    for (size_t i = 0; i < files.size(); ++i)
    {
        bool isLastFile = i == files.size() - 1;
        failed += !runFile(runtime, files[i].c_str(), L);
    }

    return failed ? 1 : 0;
}
