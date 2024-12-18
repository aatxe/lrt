#include "queijo/runtime.h"

#include "uv.h"

#include <assert.h>

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
