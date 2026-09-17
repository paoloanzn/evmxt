#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "engine.h"
#include "hex.h"

static inline uint64_t bytes_to_uint64(const uint8_t *bytes, size_t len)
{
    uint64_t value = 0;

    for (size_t i = 0; i < len; i++) {
        value = (value << 8) | bytes[i];
    }

    return value;
}

int main() {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	if (luaL_loadfile(L, "config.lua") || lua_pcall(L, 0, 0, 0)) {
		printf("Some errors were found: %s\n",
			lua_tostring(L, -1));
			lua_close(L);
			return 1;
	}
	// lua_stack = []

	lua_getglobal(L, "config");
	if (!lua_istable(L, -1)) {
		printf("config is not a table\n");
		lua_pop(L, 1);
		lua_close(L);
		return 1;
	}
	// lua_stack = [config]

	lua_getfield(L, -1, "rpc_url");
	if (!lua_isstring(L, -1)) {
	    printf("rpc_url is not a string\n");
	    lua_pop(L, 2);
	    lua_close(L);
	    return 1;
	}
	// lua_stack = [rpc_url, config]

	const char *rpc_url = lua_tostring(L, -1);
	char *chain_id;
	chain_id = rpc_call(rpc_url, "eth_chainId", "[]");

	if (!chain_id) {
		printf("Error calling %s\n", rpc_url);
		lua_pop(L, 2);
		lua_close(L);
		return 1;
	}

	uint8_t decode_buf[4096];
	int n = hex_decode(chain_id, decode_buf, sizeof(decode_buf));
	if (n < 0) {
		printf("Error decoding chainId %s\n", chain_id);
		lua_pop(L, 2);
		lua_close(L);
		return 1;
	}
	printf("chain_id = %s = %lu\n", chain_id, bytes_to_uint64(decode_buf, n));
	
	lua_pop(L, 2);
	lua_close(L);
	return 0;
}

