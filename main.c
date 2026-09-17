#include <assert.h>
#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "engine.h"

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

	printf("ChainID for configured RPC = %s\n", chain_id);
	
	lua_pop(L, 2);
	lua_close(L);
	return 0;
}

