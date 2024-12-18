#pragma once

#include "queijo/ref.h"

#include <functional>
#include <mutex>
#include <vector>

struct ThreadToContinue
{
    bool success = false;
    Ref ref;
    int argumentCount = 0;
};

struct Runtime
{
    bool hasContinuations();
    void scheduleLuauContinuation(std::function<bool(lua_State*)> cb);

    lua_State* GL = nullptr;

    std::mutex continuationMutex;
    std::vector<std::function<void()>> continuations;

    std::vector<ThreadToContinue> runningThreads;
};

Runtime* getRuntime(lua_State* L);
