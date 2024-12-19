#pragma once

#include "queijo/ref.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct ThreadToContinue
{
    bool success = false;
    std::shared_ptr<Ref> ref;
    int argumentCount = 0;
    std::function<void()> cont;
};

struct Runtime
{
    Runtime();
    ~Runtime();

    bool runToCompletion();

    // For child runtimes, run a thread waiting for work
    void runContinuously();

    bool hasContinuations();

    void schedule(std::function<void()> f);

    // Resume thread with the specified error
    void scheduleLuauError(std::shared_ptr<Ref> ref, std::string error);

    // Resume thread with the results computed by the continuation
    void scheduleLuauResume(std::shared_ptr<Ref> ref, std::function<int(lua_State*)> cont);

    // Run 'f' in a libuv work queue
    void runInWorkQueue(std::function<void()> f);

    void addPendingToken();
    void releasePendingToken();

    // VM for this runtime
    std::unique_ptr<lua_State, void (*)(lua_State*)> globalState;

    // Shorthand for global state
    lua_State* GL = nullptr;

    std::mutex dataCopyMutex;
    std::unique_ptr<lua_State, void (*)(lua_State*)> dataCopy;

    std::vector<ThreadToContinue> runningThreads;

private:
    std::mutex continuationMutex;
    std::vector<std::function<void()>> continuations;

    // TODO: can this be handled by libuv?
    std::atomic<bool> stop;
    std::condition_variable runLoopCv;
    std::thread runLoopThread;

    std::atomic<int> activeTokens;
};

Runtime* getRuntime(lua_State* L);

struct ResumeTokenData;
using ResumeToken = std::shared_ptr<ResumeTokenData>;

struct ResumeTokenData
{
    static ResumeToken get(lua_State* L);

    void fail(std::string error);
    void complete(std::function<int(lua_State*)> cont);

    Runtime* runtime = nullptr;
    std::shared_ptr<Ref> ref;
    bool completed = false;
};

ResumeToken getResumeToken(lua_State* L);
