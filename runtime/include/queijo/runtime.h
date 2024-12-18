#pragma once

#include "queijo/ref.h"

#include <functional>
#include <memory>
#include <mutex>
#include <vector>

struct ThreadToContinue
{
    bool success = false;
    std::shared_ptr<Ref> ref;
    int argumentCount = 0;
};

struct Runtime
{
    Runtime();

    bool runToCompletion();

    bool hasContinuations();

    // Resume thread with the specified error
    void scheduleLuauError(std::shared_ptr<Ref> ref, std::string error);

    // Resume thread with the results computed by the continuation
    void scheduleLuauResume(std::shared_ptr<Ref> ref, std::function<int(lua_State*)> cont);

    // Run 'f' in a libuv work queue
    void runInWorkQueue(std::function<void()> f);

    // VM for this runtime
    std::unique_ptr<lua_State, void (*)(lua_State*)> globalState;

    // Short-hand for global state
    lua_State* GL = nullptr;

    std::mutex continuationMutex;
    std::vector<std::function<void()>> continuations;

    std::vector<ThreadToContinue> runningThreads;

    std::vector<std::shared_ptr<Runtime>> childRuntimes;
};

Runtime* getRuntime(lua_State* L);
