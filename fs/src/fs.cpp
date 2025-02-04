#include "lute/fs.h"

#include "lua.h"
#include "lualib.h"
#include "uv.h"

#include "lute/ref.h"
#include "lute/runtime.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/fcntl.h> // on mac we do this
#endif
#include <sys/stat.h>
#include <string>
#include <stdlib.h>

namespace fs
{

std::optional<int> setFlags(const char* c, int* openFlags)
{
    int modeFlags = 0x0000;

    for (const char* it = c; *it != '\0'; it++)
    {
        char c = *it;
        switch (c)
        {
        case 'r':
            *openFlags |= O_RDONLY;
            break;
        case 'w':
            *openFlags |= O_WRONLY | O_TRUNC;
            break;
        case 'x':
            *openFlags |= O_CREAT | O_EXCL;
            modeFlags = 0700;
            break;
        case 'a':
            *openFlags |= O_WRONLY | O_APPEND;
            break;
        case '+':
            // If we have not set the truncate bit in 'w' mode,
            *openFlags &= ~O_RDONLY;
            *openFlags &= ~O_WRONLY;
            *openFlags |= O_RDWR;

            if ((*openFlags & O_TRUNC))
            {
                *openFlags |= O_CREAT;
                modeFlags = 0000700 | 0000070 | 0000007;
            }
            break;
        default:
            return std::nullopt;
        }
    }

    return modeFlags;
}

struct FileHandle
{
    ssize_t fileDescriptor = -1;
    int errcode = -1;
};

void setfield(lua_State* L, const char* index, int value)
{
    lua_pushstring(L, index);
    lua_pushinteger(L, value);
    lua_settable(L, -3);
}

void createFileHandle(lua_State* L, const FileHandle& toCreate)
{
    lua_newtable(L);
    setfield(L, "fd", toCreate.fileDescriptor);
    setfield(L, "err", toCreate.errcode);
}

FileHandle unpackFileHandle(lua_State* L)
{
    FileHandle result;

    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "fd");
    lua_getfield(L, 1, "err");

    ssize_t fd = luaL_checkinteger(L, -2);
    int err = luaL_checknumber(L, -1);
    result.fileDescriptor = fd;
    result.errcode = err;

    lua_pop(L, 2); // we got the args by value, so we can clean up the stack here

    return result;
}

int close(lua_State* L)
{
    lua_settop(L, 1);
    FileHandle file = unpackFileHandle(L);

    uv_fs_t closeReq;
    uv_fs_close(uv_default_loop(), &closeReq, file.fileDescriptor, nullptr);
    return 0;
}

static char readBuffer[5];
int read(lua_State* L)
{
    memset(readBuffer, 0, sizeof(readBuffer));
    // discard any extra arguments passed in
    lua_settop(L, 1);
    FileHandle file = unpackFileHandle(L);

    int numBytesRead = 0;
    uv_fs_t readReq;
    uv_buf_t iov = uv_buf_init(readBuffer, sizeof(readBuffer));
    luaL_Strbuf resultBuf;
    luaL_buffinit(L, &resultBuf);
    do
    {
        uv_fs_read(uv_default_loop(), &readReq, file.fileDescriptor, &iov, 1, -1, nullptr);

        numBytesRead = readReq.result;

        if (numBytesRead < 0)
        {
            luaL_errorL(L, "Error reading: %s. Closing file.\n", uv_err_name(numBytesRead));
            memset(readBuffer, 0, sizeof(readBuffer));
            return 0;
        }
        // concatenate the read string into the result buffer
        if (numBytesRead > 0)
            luaL_addlstring(&resultBuf, readBuffer, numBytesRead);

    } while (numBytesRead > 0);

    luaL_pushresult(&resultBuf);

    // Clean up the scratch space
    memset(readBuffer, 0, sizeof(readBuffer));
    return 1;
}

int write(lua_State* L)
{
    char writeBuffer[4096];

    // Reset the write buffer
    int wbSize = sizeof(writeBuffer);
    memset(writeBuffer, 0, sizeof(writeBuffer));
    FileHandle file = unpackFileHandle(L);
    const char* stringToWrite = luaL_checkstring(L, 2);

    // Set up the buffer to write
    int numBytesLeftToWrite = strlen(stringToWrite);
    int offset = 0;
    do
    {
        // copy stringToWrite[0], numBytesLeftToWrite into write buffer

        int sizeToWrite = std::min(wbSize, numBytesLeftToWrite);
        memcpy(writeBuffer, stringToWrite + offset, sizeToWrite);
        uv_buf_t iov = uv_buf_init(writeBuffer, sizeToWrite);

        uv_fs_t writeReq;
        int bytesWritten = 0;
        uv_fs_write(uv_default_loop(), &writeReq, file.fileDescriptor, &iov, 1, -1, nullptr);
        bytesWritten = writeReq.result;

        if (bytesWritten < 0)
        {
            // Error case.
            luaL_errorL(L, "Error writing to file with descriptor %zu\n", file.fileDescriptor);
            memset(writeBuffer, 0, sizeof(writeBuffer));
            return 0;
        }


        offset += bytesWritten;
        numBytesLeftToWrite -= bytesWritten;
    } while (numBytesLeftToWrite > 0);

    return 0;
}
// Returns 0 on error, 1 otherwise
std::optional<FileHandle> openHelper(lua_State* L, const char* path, const char* mode, int* openFlags)
{
    std::optional<int> modeFlags = setFlags(mode, openFlags);
    if (!modeFlags)
        return std::nullopt;

    uv_fs_t openReq;
    int errcode = uv_fs_open(uv_default_loop(), &openReq, path, *openFlags, *modeFlags, nullptr);
    if (openReq.result < 0)
    {
        luaL_errorL(L, "Error opening file %s\n", path);
        return std::nullopt;
    }

    return FileHandle{openReq.result, errcode};
}

int open(lua_State* L)
{
    int nArgs = lua_gettop(L);
    const char* path = luaL_checkstring(L, 1);
    int openFlags = 0x0000;
    // When the number of arguments is less 2
    if (nArgs < 1)
    {
        luaL_errorL(L, "Error: no file supplied\n");
        return 0;
    }

    if (nArgs < 2)
    {
        openFlags = O_RDONLY;
    }

    const char* mode = luaL_checkstring(L, 2);
    if (std::optional<FileHandle> result = openHelper(L, path, mode, &openFlags))
    {
        createFileHandle(L, *result);
        return 1;
    }

    return 0;
}

void cleanup(char* buffer, int size, const FileHandle& handle)
{
    memset(buffer, 0, size);
    uv_fs_t closeReq;
    uv_fs_close(uv_default_loop(), &closeReq, handle.fileDescriptor, nullptr);
}

int readfiletostring(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    const char openMode[] = "r";
    int openFlags = 0x0000;
    std::optional<FileHandle> handle = openHelper(L, path, openMode, &openFlags);
    if (!handle)
    {
        luaL_errorL(L, "Error opening file for reading at %s\n", path);
        return 0;
    }

    memset(readBuffer, 0, sizeof(readBuffer));
    // discard any extra arguments passed in
    lua_settop(L, 1);

    int numBytesRead = 0;
    uv_fs_t readReq;
    uv_buf_t iov = uv_buf_init(readBuffer, sizeof(readBuffer));
    luaL_Strbuf resultBuf;
    luaL_buffinit(L, &resultBuf);
    do
    {
        uv_fs_read(uv_default_loop(), &readReq, handle->fileDescriptor, &iov, 1, -1, nullptr);

        numBytesRead = readReq.result;

        if (numBytesRead < 0)
        {
            luaL_errorL(L, "Error reading: %s. Closing file.\n", uv_err_name(numBytesRead));
            cleanup(readBuffer, sizeof(readBuffer), *handle);
            return 0;
        }
        // concatenate the read string into the result buffer
        if (numBytesRead > 0)
            luaL_addlstring(&resultBuf, readBuffer, numBytesRead);

    } while (numBytesRead > 0);

    luaL_pushresult(&resultBuf);

    // Clean up the scratch space
    cleanup(readBuffer, sizeof(readBuffer), *handle);
    return 1;
}

int writestringtofile(lua_State* L)
{
    char writeBuffer[4096];

    const char* path = luaL_checkstring(L, 1);
    const char openMode[] = "w+";
    int openFlags = 0x0000;
    std::optional<FileHandle> handle = openHelper(L, path, openMode, &openFlags);
    if (!handle)
    {
        luaL_errorL(L, "Error opening file for reading at %s\n", path);
        return 0;
    }

    int wbSize = sizeof(writeBuffer);
    memset(writeBuffer, 0, sizeof(writeBuffer));
    const char* stringToWrite = luaL_checkstring(L, 2);

    // Set up the buffer to write
    int numBytesLeftToWrite = strlen(stringToWrite);
    int offset = 0;
    uv_buf_t iov;
    do
    {
        // copy stringToWrite[0], numBytesLeftToWrite into write buffer

        int sizeToWrite = std::min(wbSize, numBytesLeftToWrite);
        memcpy(writeBuffer, stringToWrite + offset, sizeToWrite);
        iov = uv_buf_init(writeBuffer, sizeToWrite);

        uv_fs_t writeReq;
        int bytesWritten = 0;
        uv_fs_write(uv_default_loop(), &writeReq, handle->fileDescriptor, &iov, 1, -1, nullptr);
        bytesWritten = writeReq.result;

        if (bytesWritten < 0)
        {
            // Error case.
            luaL_errorL(L, "Error writing to file with descriptor %zu\n", handle->fileDescriptor);
            cleanup(writeBuffer, sizeof(writeBuffer), *handle);
            return 0;
        }


        offset += bytesWritten;
        numBytesLeftToWrite -= bytesWritten;
    } while (numBytesLeftToWrite > 0);

    cleanup(writeBuffer, sizeof(writeBuffer), *handle);
    return 0;
}

struct ResumeCaptureInformation
{
    explicit ResumeCaptureInformation(lua_State* L)
        : token(getResumeToken(L))
    {
    }

    ResumeToken token = nullptr;
};

uv_fs_t* createRequest(lua_State* L)
{
    uv_fs_t* req = new uv_fs_t();
    req->data = new ResumeCaptureInformation(L);
    return req;
}

ResumeCaptureInformation* getResumeInformation(uv_fs_t* req)
{
    return reinterpret_cast<ResumeCaptureInformation*>(req->data);
}

int readasync(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);

    uv_fs_t* openReq = createRequest(L);
    uv_fs_open(
        uv_default_loop(),
        openReq,
        path,
        O_RDONLY,
        0,
        [](uv_fs_t* req)
        {
            ResumeCaptureInformation* info = getResumeInformation(req);
            int fd = req->result;

            if (fd < 0)
            {
                info->token->fail("Error opening file");
                uv_fs_t closeReq;
                uv_fs_close(uv_default_loop(), &closeReq, fd, nullptr);
                uv_fs_req_cleanup(req);
                delete (ResumeCaptureInformation*)req->data;
                delete req;
                return;
            }

            // Allocate the destination buffer for reading
            char readBuffer[1024];
            memset(readBuffer, 0, sizeof(readBuffer));
            uv_buf_t iov = uv_buf_init(readBuffer, sizeof(readBuffer));
            // Read data
            int numBytesRead = 0;
            uv_fs_t readReq;
            // Output data
            std::vector<char> resultData;

            do
            {
                uv_fs_read(uv_default_loop(), &readReq, fd, &iov, 1, -1, nullptr);
                numBytesRead = readReq.result;

                if (numBytesRead < 0)
                {
                    uv_fs_t closeReq;
                    uv_fs_close(uv_default_loop(), &closeReq, fd, nullptr);
                    // Schedule error;
                    // Also, we should free the original request. We don't have to do this for the read req since it's sycnrhonous
                    info->token->fail("Error reading file");
                    uv_fs_req_cleanup(req);
                    delete (ResumeCaptureInformation*)req->data;
                    delete req;
                    return;
                }


                for (int i = 0; i < numBytesRead; i++)
                    resultData.push_back(readBuffer[i]);

            } while (numBytesRead > 0);

            // Push the result buffer onto the stack
            info->token->complete(
                [data = std::move(resultData)](lua_State* L)
                {
                    lua_pushlstring(L, data.data(), data.size());
                    return 1;
                }
            );

            uv_fs_t closeReq;
            uv_fs_close(uv_default_loop(), &closeReq, fd, nullptr);
            // free the read buffer as well as the resume information and the request
            delete (ResumeCaptureInformation*)req->data;
            delete req;
            return;
        }
    );

    return lua_yield(L, 0);
}

} // namespace fs

int luaopen_fs(lua_State* L)
{
    luaL_register(L, "fs", fs::lib);
    return 1;
}

int luteopen_fs(lua_State* L)
{
    lua_createtable(L, 0, std::size(fs::lib));

    for (auto& [name, func] : fs::lib)
    {
        if (!name || !func)
            break;

        lua_pushcfunction(L, func, name);
        lua_setfield(L, -2, name);
    }

    return 1;
}
