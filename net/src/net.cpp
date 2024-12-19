#include "queijo/net.h"

#include "queijo/runtime.h"

#include "curl/curl.h"

#include "lua.h"
#include "lualib.h"

#include <string>
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

    std::vector<char> data;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);

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

    auto token = getResumeToken(L);

    // TODO: add cancellations
    token->runtime->runInWorkQueue([=] {
        auto [error, data] = requestData(url);

        if (!error.empty())
        {
            token->fail("network request failed: " + error);
        }
        else
        {
            token->complete([data = std::move(data)](lua_State* L) {
                lua_pushlstring(L, data.data(), data.size());
                return 1;
            });
        }
    });

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
