#include <stdio.h>
#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "runtime.h"

int main(void)
{
    int result = EXIT_FAILURE;
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        fprintf(stderr, "Failed to create Lua state\n");
        return EXIT_FAILURE;
    }
    luaL_openlibs(L);

    if (luaL_loadfile(L, "config.lua") != LUA_OK ||
        lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char *error = lua_tostring(L, -1);
        fprintf(stderr, "Failed to load configuration: %s\n",
            error != NULL ? error : "non-string Lua error");
        goto cleanup;
    }

    lua_getglobal(L, "global_config");
    if (!lua_istable(L, -1)) {
        fprintf(stderr, "global_config must be a table\n");
        goto cleanup;
    }

    lua_getfield(L, -1, "rpc_url");
    if (lua_type(L, -1) != LUA_TSTRING) {
        fprintf(stderr, "global_config.rpc_url must be a string\n");
        goto cleanup;
    }

    lua_runtime_ctx ctx = {0};
    ctx.rpc_url = lua_tostring(L, -1);

    // Keep the configuration state alive while ctx borrows its URL string.
    start_lua_runtime(&ctx);
    result = EXIT_SUCCESS;

cleanup:
    lua_close(L);
    return result;
}
