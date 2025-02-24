#pragma once

#include "lua.h"
#include "lualib.h"

// open the library as a standard global luau library
int luaopen_fs(lua_State* L);
// open the library as a table on top of the stack
int luteopen_fs(lua_State* L);

namespace fs
{

// TODO: add the ability to open as bytes
/* Takes  path: string, a mode: 'r|a|w|x|+' (defaulting to r when omitted)
   Returns a table representing a handle to the file the state of a file {fd : number, error : ...}
 */
int open(lua_State* L);

/* Reads a file into a string. Takes a file handle obtained from openfile */
int read(lua_State* L);

/* Writes a string to a file without closing it*/
int write(lua_State* L);

/* takes a file handle into a string and then closes it */
int close(lua_State* L);

/* reads a whole file into a string and then closes it */
int readfiletostring(lua_State* L);
/* writes a st */
int writestringtofile(lua_State* L);

/* Reads a file without blocking */
int readasync(lua_State* L);

/* Removes a file */
int fs_remove(lua_State* L);

/* Creates a folder */
int fs_mkdir(lua_State* L);

/* Removes a directory */
int fs_rmdir(lua_State* L);

/* Gets the type of a file entry */
int type(lua_State* L);

/* Lists the contents of a directory */
int listdir(lua_State* L);

static const luaL_Reg lib[] = {
    /* Manual control apis - you are responsible for calling close / open*/
    {"open", open},
    {"read", read},
    {"write", write},
    {"close", close},

    {"remove", fs_remove},

    {"type", type},

    {"mkdir", fs_mkdir},
    {"listdir", listdir},
    {"rmdir", fs_rmdir},

    {"readfiletostring", readfiletostring},
    {"writestringtofile", writestringtofile},
    {"readasync", readasync},
    {NULL, NULL},
};

} // namespace fs
