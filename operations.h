#ifndef OPERATIONS_H
#define OPERATIONS_H

#include <lua.h>

#include "runtime.h"

// Operation handlers read the payload at the top of the coroutine stack and
// return 1 after pushing one response, or report an error and return 0 while
// leaving the stack unchanged so the runtime can stop the program.

// Create a wallet from a secure random private key and return a table with
// private_key, public_key, and address encoded as lowercase hex strings with
// a 0x prefix, using a 64-byte public key without the 0x04 marker.
// The payload must be nil and the wallet is not persisted by the engine.
int operation_create_wallet(lua_State *co, lua_runtime_ctx *ctx);

// Query eth_chainId at ctx->rpc_url and return the result as a Lua integer,
// updating ctx->chain_id only after validating the response fits that type.
// The payload must be nil and RPC errors or invalid quantities stop the operation.
int operation_get_chain_id(lua_State *co, lua_runtime_ctx *ctx);

#endif /* OPERATIONS_H */
