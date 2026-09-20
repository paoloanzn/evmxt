#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int operation_set_wallet(lua_State *co, lua_runtime_ctx *ctx)
{
    // lua_stack = [operation, index, data]
    if (ctx == NULL || lua_type(co, -1) != LUA_TSTRING) {
        printf("(c) error: set_wallet requires a private_key\n");
        return 0;
    }

    size_t length;
    const char *key = lua_tolstring(co, -1, &length);
    uint8_t private_key[32], address[20];

    // Perform checks to ensure that private_key is valid and
    // derive its 20 bytes address.
    if (length < 2 || memchr(key, '\0', length) ||
        hex_decode(key, private_key, sizeof(private_key)) != 32 ||
        derive_address(private_key, address) != 0) {
        OPENSSL_cleanse(private_key, sizeof(private_key));
        printf("(c) error: invalid private_key or failed address derivation\n");
        return 0;
    }

    memcpy(ctx->private_key, private_key, sizeof(private_key));
    memcpy(ctx->address, address, sizeof(address));
    ctx->wallet_set = true;
    OPENSSL_cleanse(private_key, sizeof(private_key));

    lua_pushboolean(co, 1);
    return 1;
}

int operation_get_nonce(lua_State *co, lua_runtime_ctx *ctx)
{
    // lua_stack = [operation, index, data]
    if (ctx == NULL || !lua_isnil(co, -1)) {
        printf("(c) error: get_nonce requires a runtime context and no data\n");
        return 0;
    }
    if (!ctx->wallet_set) {
        lua_pushinteger(co, -1);
        return 1;
    }
    if (ctx->rpc_url == NULL || ctx->rpc_url[0] == '\0') {
        printf("(c) error: get_nonce requires an RPC URL\n");
        return 0;
    }

    // Both the address and block tag have fixed lengths, so this buffer is bounded.
    char address[43];
    char params[sizeof("[\"\",\"pending\"]") + 42];
    hex_encode(ctx->address, sizeof(ctx->address), address);
    snprintf(params, sizeof(params), "[\"%s\",\"pending\"]", address);

    char *response = rpc_call(ctx->rpc_url, "eth_getTransactionCount", params);
    if (response == NULL) {
        printf("(c) error: Failed to query eth_getTransactionCount\n");
        return 0;
    }

    uint8_t bytes[sizeof(uint64_t)];
    size_t length = strlen(response);
    int count = -1;
    if (length >= 3 && length <= 18 && response[0] == '0' && response[1] == 'x' &&
        (length == 3 || response[2] != '0')) {
        count = hex_decode(response, bytes, sizeof(bytes));
    }
    free(response);

    if (count <= 0) {
        printf("(c) error: Invalid nonce returned by eth_getTransactionCount\n");
        return 0;
    }

    uint64_t nonce = bytes_to_uint64(bytes, (size_t)count);
    if (nonce > (uint64_t)LUA_MAXINTEGER) {
        printf("(c) error: Nonce exceeds the Lua integer range\n");
        return 0;
    }

    ctx->nonce = nonce;
    lua_pushinteger(co, (lua_Integer)nonce);
    return 1;
}
