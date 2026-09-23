#ifndef LUA_ABI_H
#define LUA_ABI_H

#include <stddef.h>
#include <lua.h>
#include "abi.h"

// Protocol IDs shared with lua-lib/compiler.lua, independent of abi_value_kind.
typedef enum {
    LUA_ABI_ADDRESS = 1,
    LUA_ABI_BOOL = 2,
    LUA_ABI_UINT = 3,
    LUA_ABI_INT = 4,
    LUA_ABI_FIXED_BYTES = 5,
    LUA_ABI_BYTES = 6,
    LUA_ABI_STRING = 7,
    LUA_ABI_ARRAY = 8,
    LUA_ABI_FIXED_ARRAY = 9,
    LUA_ABI_TUPLE = 10
} lua_abi_kind;

// This function turns a compiled Call and its argument values into the bytes
// needed for an Ethereum call, leaving the Lua stack as it was when it started.
// Give it an empty out buffer, and if it returns 1, that buffer holds the result
// and must be freed with abi_buffer_free() when you are finished using it.
// If something goes wrong, it returns 0 and clears out, and if you provide an
// error buffer with room to write, it puts an explanation there.
//
// Keep the Call's descriptor tables and values available to Lua until this
// function finishes, because it reads from them directly rather than making
// copies, and it reads table entries without running any Lua metamethods.
// Signed numbers must be Lua integers, while unsigned numbers can also be
// written as 0x hex strings containing up to 32 bytes, and bytesN, bytes and
// string arguments use the bytes in a Lua string directly without hex decoding.
int lua_abi_encode_call(lua_State *L, int call_index, abi_buffer *out,
                        char *error, size_t error_capacity);

#endif
