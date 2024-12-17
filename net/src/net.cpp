#include "queijo/net.h"

#include "curl/curl.h"

#include "lua.h"
#include "lualib.h"

#include <vector>

static size_t writeFunction(void* contents, size_t size, size_t nmemb, void* context)
{
    std::vector<char>& target = *(std::vector<char>*)context;
    size_t fullsize = size * nmemb;

    target.insert(target.end(), (char*)contents, (char*)contents + fullsize);

    return fullsize;
}

static int net_get(lua_State* L)
{
    const char* url = luaL_checkstring(L, 1);

    CURL* curl = curl_easy_init();

    if (!curl)
        luaL_error(L, "failed to initialize the request");

    std::vector<char> data;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunction);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK)
        luaL_error(L, "network request failed: %s\n", curl_easy_strerror(res));

    curl_easy_cleanup(curl);

    lua_pushlstring(L, data.data(), data.size());
    return 1;
}

static const luaL_Reg netlib[] = {
    {"get", net_get},
    {nullptr, nullptr},
};

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

    luaL_register(L, "net", netlib);

    return 1;
}
