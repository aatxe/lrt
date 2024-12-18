#include "queijo/net.h"

#include "queijo/runtime.h"

#include "curl/curl.h"

#include "uv.h"

#include "lua.h"
#include "lualib.h"

#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace net
{

static size_t writeFunction(void* contents, size_t size, size_t nmemb, void* context)
{
    std::vector<char>& target = *(std::vector<char>*)context;
    size_t fullsize = size * nmemb;

    target.insert(target.end(), (char*)contents, (char*)contents + fullsize);

    return fullsize;
}

static std::pair<std::string, std::vector<char>> requestData(const std::string& url)
{
    CURL* curl = curl_easy_init();

    if (!curl)
        return { "failed to initialize", {} };

    printf("Requested %s at %d\n", url.c_str(), clock());

    std::vector<char> data;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);

    printf("Finished %s at %d\n", url.c_str(), clock());

    if (res != CURLE_OK)
        return { curl_easy_strerror(res), {} };

    curl_easy_cleanup(curl);
    return { "", data };
}

static int get(lua_State* L)
{
    std::string url = luaL_checkstring(L, 1);

    auto [error, data] = requestData(url);

    if (!error.empty())
        luaL_error(L, "network request failed: %s", error.c_str());

    lua_pushlstring(L, data.data(), data.size());
    return 1;
}

static int getAsync(lua_State* L)
{
    std::string url = luaL_checkstring(L, 1);

    auto ref = getRefForThread(L);
    Runtime* runtime = getRuntime(L);

    auto loop = uv_default_loop();

    struct WorkData
    {
        decltype(ref) ref;
        decltype(runtime) runtime;
        decltype(url) url;
    };

    uv_work_t *work = new uv_work_t();
    work->data = new WorkData{ ref, runtime, url };

    uv_queue_work(loop, work, [](uv_work_t* req) {
        WorkData& workData = *(WorkData*)req->data;

        auto [error, data] = requestData(workData.url);

        if (!error.empty())
        {
            workData.runtime->scheduleLuauError(workData.ref, "network request failed: " + error);
        }
        else
        {
            workData.runtime->scheduleLuauResume(workData.ref, [data = std::move(data)](lua_State* L) {
                lua_pushlstring(L, data.data(), data.size());
                return 1;
                });
        }
    }, [](uv_work_t* req, int status) {
        delete (WorkData*)req->data;
        delete req;
    });

    // TODO: switch to libuv, add cancellations
    /*std::thread thread = std::thread([=] {
        auto [error, data] = requestData(url);

        if (!error.empty())
        {
            runtime->scheduleLuauError(ref, "network request failed: " + error);
        }
        else
        {
            runtime->scheduleLuauResume(ref, [data = std::move(data)](lua_State* L) {
                lua_pushlstring(L, data.data(), data.size());
                return 1;
            });
        }
    });

    thread.detach();*/

    return lua_yield(L, 0);
}

static const luaL_Reg lib[] = {
    {"get", get},
    {"getAsync", getAsync},
    {nullptr, nullptr},
};
}

struct CurlHolder
{
    CurlHolder()
    {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    ~CurlHolder()
    {
        curl_global_cleanup();
    }
};

static CurlHolder& globalCurlInit()
{
    static CurlHolder holder;
    return holder;
}

int luaopen_net(lua_State* L)
{
    globalCurlInit();

    luaL_register(L, "net", net::lib);

    return 1;
}
