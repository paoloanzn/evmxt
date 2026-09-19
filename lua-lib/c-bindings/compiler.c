#include <lauxlib.h>
#include "selector.h"

static int selector(lua_State *L)
{
    const char *signature = luaL_checkstring(L, 1);
    uint8_t bytes[4];

    compute_selector(signature, bytes);
    lua_pushlstring(L, (const char *)bytes, sizeof(bytes));
    return 1;
}

int luaopen_compiler_native(lua_State *L)
{
    static const luaL_Reg functions[] = {
        { "selector", selector },
        { NULL, NULL }
    };
    luaL_newlib(L, functions);
    return 1;
}
