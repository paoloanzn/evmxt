#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdlib.h>

#include "rpc.h"
#include "eth_crypto.h"
#include "hex.h"
#include "selector.h"

static inline uint64_t bytes_to_uint64(const uint8_t *bytes, size_t len)
{
    uint64_t value = 0;

    for (size_t i = 0; i < len; i++) {
        value = (value << 8) | bytes[i];
    }

    return value;
}

static inline void bytes32_to_hex(const uint8_t bytes[32], char out[65])
{
    static const char hex[] = "0123456789abcdef";

    for (size_t i = 0; i < 32; ++i) {
        out[i * 2]     = hex[bytes[i] >> 4];
        out[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }

    out[64] = '\0';
}

int main() {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	if (luaL_loadfile(L, "configuration/config.lua") || lua_pcall(L, 0, 0, 0)) {
		printf("Some errors were found: %s\n",
			lua_tostring(L, -1));
			lua_close(L);
			return 1;
	}
	// lua_stack = []

	lua_getglobal(L, "global_config");
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

	uint8_t selector[4];
	char hex_encoded_selector[11];
	compute_selector("approve(address,uint256)", 
		selector);
	hex_encode(selector, 4, hex_encoded_selector);
	printf("Selector: %s\n", hex_encoded_selector);

	uint8_t private_key[32];
	n = hex_decode(
		// well known "public" private key used for testing.
		"ac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80",
		private_key, 32
	);
	uint8_t address[20];
	if (n != sizeof(private_key) || derive_address(private_key, address) != 0) {
		fprintf(stderr, "Error deriving address\n");
		free(chain_id);
		lua_pop(L, 2);
		lua_close(L);
		return 1;
	}
	char address_hex_encoded[44];
	hex_encode(address, 20, address_hex_encoded);
	printf("address: %s\n", address_hex_encoded);

	// Example hashed tx: 0x000....1
	uint8_t hash[32] = {0};
	hash[31] = 1;
	uint8_t result[32], s[32];
	int v;
	sign_hash(private_key, hash, result, s, &v);
	printf("v: %d\n", v);
	char result_hex_encoded[68];
	hex_encode(result, 32, result_hex_encoded);
	printf("result: %s\n", result_hex_encoded);


	free(chain_id);
	lua_pop(L, 2);
	lua_close(L);
	return 0;
}
