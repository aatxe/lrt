#include "Luau/Common.h"
#include "lua.h"
#include "lualib.h"

#include "Luau/CodeGen.h"
#include "Luau/Compiler.h"
#include "Luau/Parser.h"

#include "FileUtils.h"

#include "queijo/net.h"

#ifdef _WIN32
#include <Windows.h>
#endif

static bool codegen = false;
static int program_argc = 0;
char** program_argv = nullptr;

static Luau::CompileOptions copts()
{
    Luau::CompileOptions result = {};
    result.optimizationLevel = 2;
    result.debugLevel = 0;
    result.typeInfoLevel = 1;
    result.coverageLevel = 0;

    return result;
}

void load_queijo_runtime(lua_State* L, const luaL_Reg* libs)
{
    const luaL_Reg* lib = libs;
    for (; lib->func; lib++)
    {
        lua_pushcfunction(L, lib->func, NULL);
        lua_pushstring(L, lib->name);
        lua_call(L, 1, 0);
    }
}

void setupState(lua_State* L)
{
    /* register new libraries */
    if (Luau::CodeGen::isSupported())
        Luau::CodeGen::create(L);

    // register the builtin tables
    luaL_openlibs(L);

    luaopen_net(L);

    static const luaL_Reg funcs[] = {
        {NULL, NULL},
    };

    load_queijo_runtime(L, funcs);

    luaL_sandbox(L);
}

void setupArguments(lua_State* L, int argc, char** argv)
{
    lua_checkstack(L, argc);

    for (int i = 0; i < argc; ++i)
        lua_pushstring(L, argv[i]);
}

// `repl` is used it indicate if a repl should be started after executing the file.
static bool runFile(const char* name, lua_State* GL)
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
    int status = 0;

    if (luau_load(L, chunkname.c_str(), bytecode.data(), bytecode.size(), 0) == 0)
    {
        if (codegen)
        {
            Luau::CodeGen::CompilationOptions nativeOptions;
            Luau::CodeGen::compile(L, -1, nativeOptions);
        }

        setupArguments(L, program_argc, program_argv);
        status = lua_resume(L, NULL, program_argc);
    }
    else
    {
        status = LUA_ERRSYNTAX;
    }

    if (status != 0)
    {
        std::string error;

        if (status == LUA_YIELD)
        {
            error = "thread yielded unexpectedly";
        }
        else if (const char* str = lua_tostring(L, -1))
        {
            error = str;
        }

        error += "\nstacktrace:\n";
        error += lua_debugtrace(L);

        fprintf(stderr, "%s", error.c_str());
    }

    lua_pop(GL, 1);
    return status == 0;
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

    setupState(L);

    int failed = 0;

    for (size_t i = 0; i < files.size(); ++i)
    {
        bool isLastFile = i == files.size() - 1;
        failed += !runFile(files[i].c_str(), L);
    }

    return failed ? 1 : 0;
}
