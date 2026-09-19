#include <stdio.h>
#include <stdlib.h>
#include <openssl/crypto.h>

#include "operations.h"
#include "eth_crypto.h"
#include "hex.h"
#include "rpc.h"

// Convert up to eight big-endian bytes into an unsigned 64-bit integer.
static inline uint64_t bytes_to_uint64(const uint8_t *bytes, size_t len)
{
    uint64_t value = 0;
    for (size_t i = 0; i < len; ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

int operation_create_wallet(lua_State *co, lua_runtime_ctx *ctx)
{
    // lua_stack = [operation, index, data]
    (void)ctx;
    if (!lua_isnil(co, -1)) {
        printf("(c) error: wallet_create does not accept data\n");
        return 0;
    }

    uint8_t private_key[32], public_key[64], address[20];
    if (generate_eth_wallet(private_key, 
            public_key, address) != 0) {
        OPENSSL_cleanse(private_key, sizeof(private_key));
        printf("(c) error: Failed to create wallet\n");
        return 0;
    }

    char private_key_hex[67], public_key_hex[131], address_hex[43];

    hex_encode(private_key, 
        sizeof(private_key), private_key_hex);
    hex_encode(public_key, 
        sizeof(public_key), public_key_hex);
    hex_encode(address, 
        sizeof(address), address_hex);

    OPENSSL_cleanse(private_key, sizeof(private_key));

    lua_createtable(co, 0, 3);
    // lua_stack = [operation, index, data, wallet]
    lua_pushstring(co, private_key_hex);
    // lua_stack = [operation, index, data, wallet, private_key]
    OPENSSL_cleanse(private_key_hex, sizeof(private_key_hex));
    lua_setfield(co, -2, "private_key");
    // lua_stack = [operation, index, data, wallet]
    lua_pushstring(co, public_key_hex);
    // lua_stack = [operation, index, data, wallet, public_key]
    lua_setfield(co, -2, "public_key");
    // lua_stack = [operation, index, data, wallet]
    lua_pushstring(co, address_hex);
    // lua_stack = [operation, index, data, wallet, address]
    lua_setfield(co, -2, "address");
    // lua_stack = [operation, index, data, wallet]
    return 1;
}

int operation_get_chain_id(lua_State *co, lua_runtime_ctx *ctx)
{
    // lua_stack = [operation, index, data]
    if (!lua_isnil(co, -1)) {
        printf("(c) error: get_chain_id does not accept data\n");
        return 0;
    }
    if (ctx == NULL || ctx->rpc_url == NULL || ctx->rpc_url[0] == '\0') {
        printf("(c) error: get_chain_id requires an RPC URL\n");
        return 0;
    }

    char *response = rpc_call(ctx->rpc_url, "eth_chainId", "[]");
    if (response == NULL) {
        printf("(c) error: Failed to query eth_chainId\n");
        return 0;
    }

    uint8_t bytes[sizeof(uint64_t)];
    int count = response[0] == '\0' ? 
        -1 : hex_decode(response, bytes, sizeof(bytes));
    free(response);
    if (count <= 0) {
        printf("(c) error: Failed to decode eth_chainId\n");
        return 0;
    }

    uint64_t chain_id = bytes_to_uint64(bytes, count);
    if (chain_id > (uint64_t)LUA_MAXINTEGER) {
        printf("(c) error: Chain ID exceeds the Lua integer range\n");
        return 0;
    }

    lua_pushinteger(co, (lua_Integer)chain_id);
    // lua_stack = [operation, index, data, chain_id]
    ctx->chain_id = chain_id;
    return 1;
}
