#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <lua.h>

#include "runtime.h"

// Operation handlers read the payload at the top of the coroutine stack and
// return 1 after pushing one response, or report an error and return 0 while
// leaving the stack unchanged so the runtime can stop the program.

// Encode the compiled Call payload, build and sign an EIP-1559 transaction
// using the active runtime wallet and transaction settings, broadcast it with
// eth_sendRawTransaction, and return the resulting transaction hash.
int operation_call(lua_State *co, lua_runtime_ctx *ctx);

// Execute a compiled Call against latest state without submitting a transaction.
// Use the active wallet's address as sender when set, and return raw 0x result
// data, including 0x for an empty result. No runtime settings are changed.
int operation_eth_call(lua_State *co, lua_runtime_ctx *ctx);

// Create a wallet from a secure random private key and return a table with
// private_key, public_key, and address encoded as lowercase hex strings with
// a 0x prefix, using a 64-byte public key without the 0x04 marker.
// The payload must be nil and the wallet is not persisted by the engine.
int operation_create_wallet(lua_State *co, lua_runtime_ctx *ctx);

// Query eth_chainId at ctx->rpc_url and return the result as a Lua integer,
// updating ctx->chain_id only after validating the response fits that type.
// The payload must be nil and RPC errors or invalid quantities stop the operation.
int operation_get_chain_id(lua_State *co, lua_runtime_ctx *ctx);

// Set the active wallet's private key and derived address together, leaving
// the previous wallet unchanged if the key cannot be decoded or used.
int operation_set_wallet(lua_State *co, lua_runtime_ctx *ctx);

// Fetch the active wallet's pending nonce, store it in ctx and return it to Lua,
// or return -1 without an RPC request when no wallet has been set.
int operation_get_nonce(lua_State *co, lua_runtime_ctx *ctx);

// Query eth_getBalance for the active wallet at "latest" and return its wei
// balance as a validated 0x quantity string (up to 256 bits). No payload or
// runtime state changes; requires an active wallet.
int operation_get_balance(lua_State *co, lua_runtime_ctx *ctx);

// Copy a validated Gas object's limit and 32-byte big-endian fees into ctx
// and return true, leaving the previous settings intact if its storage is invalid.
int operation_set_gas(lua_State *co, lua_runtime_ctx *ctx);

#endif /* OPERATIONS_H */
