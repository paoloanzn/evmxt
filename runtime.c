#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "runtime.h"

#define PROGRAM_FILEPATH "program.lua"
#define NEXT() goto *dispatch[*pc++]

typedef enum {
    OP_CALL = 0,
    OP_WALLET_CREATE = 1,
    OP_GET_CHAIN_ID = 2,
    OP_COUNT = 3,
    OP_HALT = OP_COUNT, // Internal instruction; never accepted from Lua.
} op;


// Return zero when dispatch fails, so the caller can stop the runtime.
static int handle_operation(lua_State *co, lua_Integer index)
{
    // lua_stack = [yielded operation, index, data]
    (void)co;

    // GCC/Clang labels-as-values; indices match lua-lib/ops.lua.
    static void *const dispatch[] = {
        [OP_CALL] = &&op_call,
        [OP_WALLET_CREATE] = &&op_wallet_create,
        [OP_GET_CHAIN_ID] = &&op_get_chain_id,
        [OP_HALT] = &&op_halt,
    };
    if (index < 0 || index >= OP_COUNT) {
        goto op_invalid;
    }

    // Each Lua yield supplies one operation.
    // Halt returns to the resume loop.
    const op instructions[] = { (op)index, OP_HALT };
    const op *pc = instructions;
    NEXT();

op_call:
    // TODO: Process call data.
    NEXT();
op_wallet_create:
    // TODO: Create a wallet.
    NEXT();
op_get_chain_id:
    // TODO: Return the chain ID.
    NEXT();
op_halt:
    return 1;
op_invalid:
    printf("(c) error: Unknown operation index\n");
    return 0;
}

#undef NEXT

void start_lua_runtime(lua_runtime_ctx *ctx)
{
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    lua_State *co = lua_newthread(L);

    if (luaL_loadfile(co, PROGRAM_FILEPATH)) {
        printf("(lua) error: Error loading %s: %s\n", PROGRAM_FILEPATH, 
            lua_tostring(co, -1));
        goto cleanup;
    }

    if (lua_pcall(co, 0, 1, 0)) {
        printf("(lua) error: Error running %s: %s\n", PROGRAM_FILEPATH, 
            lua_tostring(co, -1));
        goto cleanup;
    }
    // lua_stack = [{main}]
    
    // '{main}' must be a table
    if (!lua_istable(co, -1)) {
        printf("(c) error: %s must return a table\n", PROGRAM_FILEPATH);
        goto cleanup;
    }

    lua_getfield(co, -1, "main");
    // lua_stack = [{main}, main]
    lua_remove(co, -2);
    // lua_stack = [main]

    if(!lua_isfunction(co, -1)) {
        printf("(c) error: %s must return a table containing \
            'main = function() ... end'\n", PROGRAM_FILEPATH);
        goto cleanup;
    }

    // Run `main = function ... end` until it returns or crashes
    int nargs = 0;
    for (;;) {
        int nresults = 0;
        int status = lua_resume(co, NULL, nargs, &nresults);

        if (status == LUA_YIELD) {
            // We always expect 1 argument at time to be yieled.
            // That argument must be a table of type Op, (see 
            // `lua-lib/ops.lua`).
            if (nresults != 1) { 
                printf("(c) error: Unexpected number of \
                    yielded values: %d\n", nresults);
                break;
            }
            
            if (!lua_istable(co, -1)) {
                printf("(c) error: yielded value must be table\n");
                break;
            }

            lua_getfield(co, -1, "index");
            lua_getfield(co, -2, "data");
            // lua_stack = [yielded operation, index, data]
            if (!lua_isinteger(co, -2)) {
                printf("(c) error: Op.index must be an integer\n");
                break;
            }

            lua_Integer index = lua_tointeger(co, -2);
            if (!handle_operation(co, index)) {
                break;
            }

            lua_pop(co, nresults + 2);
            lua_pushinteger(co, 24);
            nargs = 1;
        } else if (status == LUA_OK) {
            break;
        } else {
            printf("(lua) error: %s\n", lua_tostring(co, -1));
            break;
        }

        // Run until `main = function ... end` ends.
        continue;

    }
    goto cleanup;

    cleanup:
        lua_close(L);
        return;
}
