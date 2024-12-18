#pragma once

#include "queijo/ref.h"

#include <functional>
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
    bool hasContinuations();

    // Resume thread with the specified error
    void scheduleLuauError(std::shared_ptr<Ref> ref, std::string error);

    // Resume thread with the results computed by the continuation
    void scheduleLuauResume(std::shared_ptr<Ref> ref, std::function<int(lua_State*)> cont);

    lua_State* GL = nullptr;

    std::mutex continuationMutex;
    std::vector<std::function<void()>> continuations;

    std::vector<ThreadToContinue> runningThreads;
};

Runtime* getRuntime(lua_State* L);
