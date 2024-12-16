#pragma once

#include "lua.h"
#include "lualib.h"

// TODO: add the ability to open as bytes
/* Takes  path: string, a mode: 'r|a|w|x|+' (defaulting to r when omitted)
   Returns a table representing a handle to the file the state of a file {fd : number, error : ...}
 */
int openfile(lua_State* L);

/* Reads a file into a string and then closes it. Takes a file handle obtained from openfile */
int readtostring(lua_State* L);

/* Writes a string to a file without closing it*/
int writestringtofile(lua_State* L);

/* takes a file handle into a string and then closes it */
int close(lua_State* L);

// Synchronous file access apis (BAD!!)
static const luaL_Reg fslib[] = {
    {"openfile", openfile},
    {"readtostring", readtostring},
    {"writestringtofile", writestringtofile},
    {"close", close},
    {NULL, NULL},
};

int luaopen_fs(lua_State* L);
