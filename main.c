#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

int main() {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	luaL_loadfile(L, "init.lua");
	lua_pcall(L, 0, 0, 0);
	lua_close(L);
	return 0;
}

