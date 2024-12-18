#include "queijo/runtime.h"

#include "lua.h"

#include "uv.h"

#include <assert.h>

static void lua_close_checked(lua_State* L)
{
    if (L)
        lua_close(L);
}

Runtime::Runtime()
    : globalState(nullptr, lua_close_checked)
{
}

bool Runtime::runToCompletion()
{
    // While there is some C++ or Luau code left to run
    while (!runningThreads.empty() || hasContinuations())
    {
        // Complete all C++ continuations
        std::vector<std::function<void()>> copy;

        {
            std::unique_lock lock(continuationMutex);
            copy = std::move(continuations);
            continuations.clear();
        }

        for (auto&& continuation : copy)
            continuation();

        if (runningThreads.empty())
            continue;

        auto next = std::move(runningThreads.front());
        runningThreads.erase(runningThreads.begin());

        next.ref->push(GL);
        lua_State* L = lua_tothread(GL, -1);

        if (L == nullptr)
        {
            fprintf(stderr, "Cannot resume a non-thread reference");
            return false;
        }

        // We still have 'next' on stack to hold on to thread we are about to run
        lua_pop(GL, 1);

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

            runningThreads.push_back({ true, getRefForThread(L), 0 });
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

bool Runtime::hasContinuations()
{
    std::unique_lock lock(continuationMutex);
    return !continuations.empty();
}

void Runtime::scheduleLuauError(std::shared_ptr<Ref> ref, std::string error)
{
    std::unique_lock lock(continuationMutex);

    continuations.push_back([this, ref, error = std::move(error)]() mutable {
        ref->push(GL);
        lua_State* L = lua_tothread(GL, -1);
        lua_pop(GL, 1);

        lua_pushlstring(L, error.data(), error.size());
        runningThreads.push_back({ false, ref, lua_gettop(L) });
    });
}

void Runtime::scheduleLuauResume(std::shared_ptr<Ref> ref, std::function<int(lua_State*)> cont)
{
    std::unique_lock lock(continuationMutex);

    continuations.push_back([this, ref, cont = std::move(cont)]() mutable {
        ref->push(GL);
        lua_State* L = lua_tothread(GL, -1);
        lua_pop(GL, 1);

        int results = cont(L);
        runningThreads.push_back({ true, ref, results });
    });
}

void Runtime::runInWorkQueue(std::function<void()> f)
{
    auto loop = uv_default_loop();

    uv_work_t* work = new uv_work_t();
    work->data = new decltype(f)(std::move(f));

    uv_queue_work(loop, work, [](uv_work_t* req) {
        auto task = *(decltype(f)*)req->data;

        task();
    }, [](uv_work_t* req, int status) {
        delete (decltype(f)*)req->data;
        delete req;
    });
}

Runtime* getRuntime(lua_State* L)
{
    return reinterpret_cast<Runtime*>(lua_getthreaddata(lua_mainthread(L)));
}
