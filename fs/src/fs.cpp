#include "queijo/fs.h"
#include "lua.h"
#include "lualib.h"
#include "queijo/ref.h"
#include "queijo/runtime.h"
#include "uv.h"
#include <cstdio>
#include <cstring>
#include <memory>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/fcntl.h> // on mac we do this
#endif
#include <sys/stat.h>
#include <stdlib.h>
#include <map>
using namespace std;

int setFlags(const char* c, int* openFlags, int* modeFlags)
{
    for (const char* it = c; *it != '\0'; it++)
    {
        char c = *it;
        switch (c)
        {
        case 'r':
        {
            *openFlags |= O_RDONLY;
            break;
        }
        case 'w':
        {
            *openFlags |= O_WRONLY | O_TRUNC;
            break;
        }
        case 'x':
        {
            *openFlags |= O_CREAT | O_EXCL;
            *modeFlags = 0700;
            break;
        }
        case 'a':
        {
            *openFlags |= O_WRONLY | O_APPEND;
        }
        case '+':
        {

            // If we have not set the truncate bit in 'w' mode,
            *openFlags &= ~O_RDONLY;
            *openFlags &= ~O_WRONLY;
            *openFlags |= O_RDWR;

            if ((*openFlags & O_TRUNC))
            {
                *openFlags |= O_CREAT;
                *modeFlags = 0000700 | 0000070 | 0000007;
            }
            break;
        }
        default:
        {
            printf("Unsupported mode %c\n", c);
            return 1;
        }
        }
    }

    return 0;
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

void unpackFileHandle(lua_State* L, FileHandle& result)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getfield(L, 1, "fd");
    lua_getfield(L, 1, "err");
    ssize_t fd = luaL_checkinteger(L, -2);
    int err = luaL_checknumber(L, -1);
    result.fileDescriptor = fd;
    result.errcode = err;
    lua_pop(L, 2); // we got the args by value, so we can clean up the stack here
}

int close(lua_State* L)
{
    lua_settop(L, 1);
    FileHandle file;
    unpackFileHandle(L, file);

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
    FileHandle file;
    unpackFileHandle(L, file);

    int numBytesRead = 0;
    int totalBytesRead = 0;
    uv_fs_t readReq;
    uv_buf_t iov = uv_buf_init(readBuffer, sizeof(readBuffer));
    luaL_Strbuf resultBuf;
    luaL_buffinit(L, &resultBuf);
    do
    {
        uv_fs_read(uv_default_loop(), &readReq, file.fileDescriptor, &iov, 1, -1, nullptr);

        numBytesRead = readReq.result;
        totalBytesRead += numBytesRead;

        if (numBytesRead < 0)
        {
            printf("Error reading: %s. Closing file.\n", uv_err_name(numBytesRead));
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


static char writeBuffer[4096];

int write(lua_State* L)
{
    // Reset the write buffer
    int wbSize = sizeof(writeBuffer);
    memset(writeBuffer, 0, sizeof(writeBuffer));
    FileHandle file;
    unpackFileHandle(L, file);
    const char* stringToWrite = luaL_checkstring(L, 2);

    // Set up the buffer to write
    int numBytesLeftToWrite = strlen(stringToWrite);

    do
    {
        int offset = 0;
        // copy stringToWrite[0], numBytesLeftToWrite into write buffer

        int sizeToWrite = min(wbSize, numBytesLeftToWrite);
        memcpy(writeBuffer, stringToWrite + offset, sizeToWrite);
        uv_buf_t iov = uv_buf_init(writeBuffer, numBytesLeftToWrite);

        uv_fs_t writeReq;
        int bytesWritten = 0;
        uv_fs_write(uv_default_loop(), &writeReq, file.fileDescriptor, &iov, 1, -1, nullptr);
        bytesWritten = writeReq.result;

        if (bytesWritten < 0)
        {
            // Error case.
            printf("Error writing to file with descriptor %zu\n", file.fileDescriptor);
            memset(writeBuffer, 0, sizeof(writeBuffer));
            return 0;
        }


        offset += bytesWritten;
        numBytesLeftToWrite -= bytesWritten;
    } while (numBytesLeftToWrite > 0);

    return 0;
}
// Returns 0 on error, 1 otherwise
int openHelper(lua_State* L, const char* path, const char* mode, int* openFlags, FileHandle& outHandle)
{
    int modeFlags = 0x0000;

    if (setFlags(mode, openFlags, &modeFlags))
    {
        return 0;
    }
    uv_fs_t openReq;
    int errcode = uv_fs_open(uv_default_loop(), &openReq, path, *openFlags, modeFlags, nullptr);
    if (openReq.result < 0)
    {
        printf("Error opening file %s\n", path);
        return 0;
    }
    FileHandle handle{openReq.result, errcode};
    outHandle.fileDescriptor = openReq.result;
    outHandle.errcode = errcode;
    return 1;
}

int open(lua_State* L)
{
    int nArgs = lua_gettop(L);
    const char* path = luaL_checkstring(L, 1);
    int openFlags = 0x0000;
    int modeFlags = 0x0000;
    // When the number of arguments is less 2
    if (nArgs < 1)
    {
        printf("Error: no file supplied\n");
        return 0;
    }

    if (nArgs < 2)
    {
        openFlags = O_RDONLY;
    }

    const char* mode = luaL_checkstring(L, 2);
    FileHandle result;
    if (openHelper(L, path, mode, &openFlags, result))
    {
        createFileHandle(L, result);
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
    FileHandle handle;
    if (!openHelper(L, path, openMode, &openFlags, handle))
    {
        printf("Error opening file for reading at %s\n", path);
        return 0;
    }

    memset(readBuffer, 0, sizeof(readBuffer));
    // discard any extra arguments passed in
    lua_settop(L, 1);

    int numBytesRead = 0;
    int totalBytesRead = 0;
    uv_fs_t readReq;
    uv_buf_t iov = uv_buf_init(readBuffer, sizeof(readBuffer));
    luaL_Strbuf resultBuf;
    luaL_buffinit(L, &resultBuf);
    do
    {
        uv_fs_read(uv_default_loop(), &readReq, handle.fileDescriptor, &iov, 1, -1, nullptr);

        numBytesRead = readReq.result;
        totalBytesRead += numBytesRead;

        if (numBytesRead < 0)
        {
            printf("Error reading: %s. Closing file.\n", uv_err_name(numBytesRead));
            cleanup(readBuffer, sizeof(readBuffer), handle);
            return 0;
        }
        // concatenate the read string into the result buffer
        if (numBytesRead > 0)
            luaL_addlstring(&resultBuf, readBuffer, numBytesRead);

    } while (numBytesRead > 0);

    luaL_pushresult(&resultBuf);

    // Clean up the scratch space
    cleanup(readBuffer, sizeof(readBuffer), handle);
    return 1;
}

int writestringtofile(lua_State* L)
{

    const char* path = luaL_checkstring(L, 1);
    const char openMode[] = "w+";
    int openFlags = 0x0000;
    FileHandle handle;
    if (!openHelper(L, path, openMode, &openFlags, handle))
    {
        printf("Error opening file for reading at %s\n", path);
        return 0;
    }

    int wbSize = sizeof(writeBuffer);
    memset(writeBuffer, 0, sizeof(writeBuffer));
    const char* stringToWrite = luaL_checkstring(L, 2);

    // Set up the buffer to write
    int numBytesLeftToWrite = strlen(stringToWrite);
    uv_buf_t iov;
    do
    {
        int offset = 0;
        // copy stringToWrite[0], numBytesLeftToWrite into write buffer

        int sizeToWrite = min(wbSize, numBytesLeftToWrite);
        memcpy(writeBuffer, stringToWrite + offset, sizeToWrite);
        iov = uv_buf_init(writeBuffer, numBytesLeftToWrite);

        uv_fs_t writeReq;
        int bytesWritten = 0;
        uv_fs_write(uv_default_loop(), &writeReq, handle.fileDescriptor, &iov, 1, -1, nullptr);
        bytesWritten = writeReq.result;

        if (bytesWritten < 0)
        {
            // Error case.
            printf("Error writing to file with descriptor %zu\n", handle.fileDescriptor);
            cleanup(writeBuffer, sizeof(writeBuffer), handle);
            return 0;
        }


        offset += bytesWritten;
        numBytesLeftToWrite -= bytesWritten;
    } while (numBytesLeftToWrite > 0);

    cleanup(writeBuffer, sizeof(writeBuffer), handle);
    return 0;
}

struct ResumeCaptureInformation
{
    explicit ResumeCaptureInformation(Runtime* rt, std::shared_ptr<Ref> ref)
        : runtime(rt)
        , ref(std::move(ref))
    {
    }
    Runtime* runtime = nullptr;
    std::shared_ptr<Ref> ref = nullptr;
};

uv_fs_t* createRequest(lua_State* L)
{
    uv_fs_t* req = new uv_fs_t();
    req->data = new ResumeCaptureInformation(getRuntime(L), std::move(getRefForThread(L)));
    return req;
}

ResumeCaptureInformation* getResumeInformation(uv_fs_t* req)
{
    return reinterpret_cast<ResumeCaptureInformation*>(req->data);
}

int readasync(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    Runtime* runtime = getRuntime(L);
    std::shared_ptr<Ref> ref = getRefForThread(L);

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
                info->runtime->scheduleLuauError(info->ref, "Error opening file");
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
            int totalBytesRead = 0;
            uv_fs_t readReq;
            // Output data
            std::vector<char> resultData;

            do
            {
                uv_fs_read(uv_default_loop(), &readReq, fd, &iov, 1, -1, nullptr);
                numBytesRead = readReq.result;
                totalBytesRead += numBytesRead;

                if (numBytesRead < 0)
                {
                    uv_fs_t closeReq;
                    uv_fs_close(uv_default_loop(), &closeReq, fd, nullptr);
                    // Schedule error;
                    // Also, we should free the original request. We don't have to do this for the read req since it's sycnrhonous
                    info->runtime->scheduleLuauError(info->ref, "Error reading file");
                    uv_fs_req_cleanup(req);
                    delete (ResumeCaptureInformation*)req->data;
                    delete req;
                    return;
                }


                for (int i = 0; i < numBytesRead; i++)
                    resultData.push_back(readBuffer[i]);

            } while (numBytesRead > 0);

            // Push the result buffer onto the stack
            info->runtime->scheduleLuauResume(
                info->ref,
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

int luaopen_fs(lua_State* L)
{
    luaL_register(L, "fs", fslib);
    return 1;
}
