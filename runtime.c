#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "runtime.h"

#define PROGRAM_FILEPATH "program.lua"

void start_lua_runtime(lua_runtime_ctx *ctx)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_State *co = lua_newthread(L);

    if (luaL_loadfile(co, PROGRAM_FILEPATH)) {
        printf("(lua) error: Error loading %s: %s\n", PROGRAM_FILEPATH, 
            lua_tostring(co, -1));
        goto cleanup;
    }

    if (lua_pcall(co, 0, 1, 0)) {
        printf("(lua) error: Error running %s: %s\n", PROGRAM_FILEPATH, 
            lua_tostring(co, -1));
        goto cleanup;
    }
    // lua_stack = [{main}]
    
    // '{main}' must be a table
    if (!lua_istable(co, -1)) {
        printf("(c) error: %s must return a table\n", PROGRAM_FILEPATH);
        goto cleanup;
    }

    lua_getfield(co, -1, "main");
    // lua_stack = [{main}, main]
    lua_remove(co, -2);
    // lua_stack = [main]

    if(!lua_isfunction(co, -1)) {
        printf("(c) error: %s must return a table containing \
            'main = function() ... end'\n", PROGRAM_FILEPATH);
        goto cleanup;
    }

    // Run `main = function ... end` until it returns or crashes
    int nargs = 0;
    for (;;) {
        int nresults = 0;
        int status = lua_resume(co, NULL, nargs, &nresults);

        if (status == LUA_YIELD) {
            // We always expect 1 argument at time to be yieled.
            // That argument must be a table of type Op, (see 
            // `lua-lib/ops.lua`).
            if (nresults != 1) { 
                printf("(c) error: Unexpected number of \
                    yielded values: %d\n", nresults);
                break;
            }
            
            if (!lua_istable(co, -1)) {
                printf("(c) error: yielded value must be table\n");
                break;
            }

            // .. process operation
            lua_pop(co, nresults);
            lua_pushinteger(co, 24);
            nargs = 1;
        } else if (status == LUA_OK) {
            break;
        } else {
            printf("(lua) error: %s\n", lua_tostring(co, -1));
            break;
        }

        // Run until `main = function ... end` ends.
        continue;

    }
    goto cleanup;

    cleanup:
        lua_close(L);
        return;
}
