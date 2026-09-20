#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/crypto.h>

#include "operations.h"
#include "eip_1559_tx.h"
#include "eth_crypto.h"
#include "hex.h"
#include "rpc.h"
#include "lua_abi.h"

int operation_call(lua_State *co, lua_runtime_ctx *ctx)
{
    // lua_stack = [operation, index, data]; restore it on every failure.
    int top = lua_gettop(co);
    abi_buffer calldata = {0};
    char error[2048] = "";
    uint8_t *raw = NULL;
    char *raw_hex = NULL, *params = NULL, *response = NULL;
    const char *failure = NULL;

    if (!ctx || !ctx->rpc_url || !ctx->rpc_url[0]) {
        failure = "call requires an RPC URL";
        goto cleanup;
    }
    if (!ctx->wallet_set) {
        failure = "call requires an active wallet";
        goto cleanup;
    }
    if (!ctx->chain_id_set) {
        failure = "call requires chain ID";
        goto cleanup;
    }
    if (!ctx->nonce_set) {
        failure = "call requires a nonce";
        goto cleanup;
    }
    if (!ctx->gas_set) {
        failure = "call requires gas settings";
        goto cleanup;
    }
    if (top < 3 || !lua_istable(co, top - 2) || !lua_checkstack(co, 2)) {
        failure = "invalid call operation stack";
        goto cleanup;
    }
    lua_pushliteral(co, "to");
    lua_rawget(co, top - 2);
    size_t to_length = 0;
    const char *to_hex = lua_type(co, -1) == LUA_TSTRING ? lua_tolstring(co, -1, &to_length) : NULL;
    uint8_t to[20];
    if (!to_hex || to_length != 42 || to_hex[0] != '0' || to_hex[1] != 'x'
        || memchr(to_hex, '\0', to_length) || hex_decode(to_hex, to, sizeof(to)) != 20) {
        failure = "call requires Op.to: 0x followed by 40 hex digits";
        goto cleanup;
    }
    if (!lua_abi_encode_call(co, top, &calldata, error, sizeof(error))) {
        failure = error;
        goto cleanup;
    }
    eip_1559_tx tx = {0};
    tx.chain_id = ctx->chain_id;
    tx.nonce = ctx->nonce;
    memcpy(tx.max_priority_fee_per_gas, ctx->max_priority_fee_per_gas,
           sizeof(tx.max_priority_fee_per_gas));
    memcpy(tx.max_fee_per_gas, ctx->max_fee_per_gas, sizeof(tx.max_fee_per_gas));
    tx.gas_limit = ctx->gas_limit;
    tx.to = to;
    tx.data = calldata.data;
    tx.data_len = calldata.len;
    tx.access_list = NULL;
    tx.access_list_count = 0;

    size_t raw_len = 0;
    eip_1559_status status =
        eip_1559_tx_encode(&tx, ctx->private_key, NULL, 0, &raw_len);
    if (status != EIP_1559_NO_SPACE) {
        snprintf(error, sizeof(error), "cannot size signed transaction (status %d)", status);
        failure = error;
        goto cleanup;
    }
    raw = malloc(raw_len);
    if (!raw) {
        failure = "cannot allocate signed transaction";
        goto cleanup;
    }
    status = eip_1559_tx_encode(&tx, ctx->private_key, raw, raw_len, &raw_len);
    if (status != EIP_1559_OK) {
        snprintf(error, sizeof(error), "cannot encode signed transaction (status %d)", status);
        failure = error;
        goto cleanup;
    }
    if (raw_len > (SIZE_MAX - 3) / 2) {
        failure = "signed transaction is too large to hex-encode";
        goto cleanup;
    }
    raw_hex = malloc(raw_len * 2 + 3);
    if (!raw_hex) {
        failure = "cannot allocate signed transaction hex string";
        goto cleanup;
    }
    hex_encode(raw, raw_len, raw_hex);

    // Generated hex requires no JSON escaping.
    const char *format = "[\"%s\"]";
    int length = snprintf(NULL, 0, format, raw_hex);
    if (length < 0 || !(params = malloc((size_t)length + 1))) {
        failure = "cannot allocate eth_sendRawTransaction parameters";
        goto cleanup;
    }
    snprintf(params, (size_t)length + 1, format, raw_hex);
    response = rpc_call(ctx->rpc_url, "eth_sendRawTransaction", params);
    if (!response) {
        failure = "eth_sendRawTransaction RPC request failed";
        goto cleanup;
    }
    uint8_t hash[32];
    if (strlen(response) != 66 || response[0] != '0' || response[1] != 'x' ||
        hex_decode(response, hash, sizeof(hash)) != 32) {
        failure = "eth_sendRawTransaction returned an invalid transaction hash";
        goto cleanup;
    }
    if (ctx->nonce == UINT64_MAX) {
        ctx->nonce_set = false;
    } else {
        ctx->nonce++;
    }

cleanup:
    free(raw);
    free(raw_hex);
    free(params);
    abi_buffer_free(&calldata);
    lua_settop(co, top);
    if (failure) {
        free(response);
        printf("(c) error: %s\n", failure);
        return 0;
    }
    lua_pushstring(co, response);
    free(response);
    return 1;
}

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
    ctx->chain_id_set = true;
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
    ctx->nonce_set = true;
    lua_pushinteger(co, (lua_Integer)nonce);
    return 1;
}

int operation_set_gas(lua_State *co, lua_runtime_ctx *ctx)
{
    // Lua Gas objects already contain validated settings and normalized fees.
    int top = lua_gettop(co);
    if (ctx == NULL || !lua_istable(co, top) || !lua_checkstack(co, 3)) {
        printf("(c) error: set_gas requires a runtime context and a Gas object\n");
        return 0;
    }

    lua_pushliteral(co, "gas_limit");
    lua_rawget(co, top);
    lua_pushliteral(co, "max_priority_fee_per_gas");
    lua_rawget(co, top);
    lua_pushliteral(co, "max_fee_per_gas");
    lua_rawget(co, top);

    size_t priority_length = 0, maximum_length = 0;
    const char *priority = lua_type(co, -2) == LUA_TSTRING
        ? lua_tolstring(co, -2, &priority_length) : NULL;
    const char *maximum = lua_type(co, -1) == LUA_TSTRING
        ? lua_tolstring(co, -1, &maximum_length) : NULL;

    // Check the storage shape before copying, in case Lua code changed the object.
    if (!lua_isinteger(co, -3) || lua_tointeger(co, -3) < 0 ||
        priority_length != 32 || maximum_length != 32) {
        lua_settop(co, top);
        printf("(c) error: invalid Gas object storage\n");
        return 0;
    }

    ctx->gas_limit = (uint64_t)lua_tointeger(co, -3);
    memcpy(ctx->max_priority_fee_per_gas, priority, 32);
    memcpy(ctx->max_fee_per_gas, maximum, 32);
    ctx->gas_set = true;
    lua_settop(co, top);
    lua_pushboolean(co, 1);
    return 1;
}
