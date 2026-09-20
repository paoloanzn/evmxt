#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint64_t chain_id;
    uint64_t nonce;
    uint64_t gas_limit;
    uint8_t max_priority_fee_per_gas[32]; // Wei/gas; must not exceed max_fee_per_gas.
    uint8_t max_fee_per_gas[32];          // Wei/gas, including tip.
    uint8_t private_key[32];
    uint8_t address[20];
    bool wallet_set;

    const char *rpc_url;
} lua_runtime_ctx;

void start_lua_runtime(lua_runtime_ctx *ctx);

#endif /* RUNTIME_H */
