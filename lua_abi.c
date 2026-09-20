#include "lua_abi.h"
#include "hex.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// We walk through the Lua values twice, first to check them and count how many
// abi_value entries we need, then to fill those entries after reserving enough
// memory for all of them at once, before passing the finished tree to abi.c
// to turn it into bytes.
//
// On the first walk, arena.values and the output pointers are NULL because we
// are only counting and have nowhere to store entries yet, while on the second
// walk each array or tuple sets aside space for all its members before visiting
// them, so those members stay next to each other in memory even when they
// contain more arrays or tuples.
typedef struct {
    abi_value *values;
    size_t capacity;
    size_t used;
} abi_arena;

typedef struct {
    abi_arena arena;
    char *error;
    size_t error_capacity;
    char path[2048];
} build_context;

static int fail(build_context *ctx, const char *format, ...)
{
    if (ctx->error && ctx->error_capacity) {
        size_t offset = 0;

        if (ctx->path[0]) {
            int length = snprintf(
                ctx->error, ctx->error_capacity, "%s: ", ctx->path);

            if (length < 0 || (size_t)length >= ctx->error_capacity) {
                return 0;
            }
            offset = (size_t)length;
        }

        va_list args;
        va_start(args, format);
        vsnprintf(ctx->error + offset, ctx->error_capacity - offset, format, args);
        va_end(args);
    }

    return 0;
}

// Raw reads avoid invoking user metamethods during validation or construction.
static void raw_field(lua_State *L, int index, const char *name)
{
    index = lua_absindex(L, index);
    lua_pushstring(L, name);
    lua_rawget(L, index);
}

static int integer_field(
    lua_State *L,
    int index,
    const char *name,
    lua_Integer *out)
{
    raw_field(L, index, name);
    int ok = lua_isinteger(L, -1);

    if (ok) {
        *out = lua_tointeger(L, -1);
    }

    lua_pop(L, 1);
    return ok;
}

static int dynamic_field(lua_State *L, int index, int *out)
{
    raw_field(L, index, "dynamic");
    int ok = lua_isboolean(L, -1);

    if (ok) {
        *out = lua_toboolean(L, -1);
    }

    lua_pop(L, 1);
    return ok;
}

// Checking all keys catches holes beyond Lua's chosen raw length boundary.
static int dense_count(
    lua_State *L,
    int index,
    size_t *count,
    build_context *ctx)
{
    index = lua_absindex(L, index);
    if (!lua_istable(L, index)) {
        return fail(ctx, "expected a dense positional table");
    }

    size_t length = lua_rawlen(L, index);
    size_t seen = 0;

    if (length > LUA_MAXINTEGER) {
        return fail(ctx, "array is too large");
    }

    lua_pushnil(L);
    while (lua_next(L, index)) {
        int valid = lua_isinteger(L, -2);
        lua_Integer key = valid ? lua_tointeger(L, -2) : 0;
        lua_pop(L, 1);

        if (!valid || key < 1 || (lua_Unsigned)key > length) {
            lua_pop(L, 1);
            return fail(ctx, "expected a dense positional table (invalid index)");
        }
        ++seen;
    }

    if (seen != length) {
        return fail(ctx, "expected a dense positional table (sparse array)");
    }

    *count = length;
    return 1;
}

static int arena_take(build_context *ctx, size_t count, abi_value **out)
{
    abi_arena *arena = &ctx->arena;

    if (count > arena->capacity - arena->used) {
        return fail(ctx, "ABI descriptor storage overflow");
    }

    *out = arena->values && count ? arena->values + arena->used : NULL;
    arena->used += count;
    return 1;
}

static int build_address(
    lua_State *L,
    int value_index,
    build_context *ctx,
    abi_value *out)
{
    if (lua_type(L, value_index) != LUA_TSTRING) {
        return fail(ctx, "expected address");
    }

    size_t length;
    const char *text = lua_tolstring(L, value_index, &length);
    uint8_t bytes[20];

    // Reject embedded NULs before passing the string to the hex utility.
    if (length != 42 || text[0] != '0' || text[1] != 'x' ||
        memchr(text, '\0', length) ||
        hex_decode(text, bytes, sizeof(bytes)) != 20) {
        return fail(ctx, "expected address: 0x followed by 40 hex digits");
    }

    *out = abi_address(bytes);
    return 1;
}

static int build_uint_hex(
    lua_State *L,
    int value_index,
    lua_Integer width,
    build_context *ctx,
    abi_value *out)
{
    size_t length;
    const char *text = lua_tolstring(L, value_index, &length);
    uint8_t bytes[32];

    if (length < 3 || length > 66 || text[0] != '0' || text[1] != 'x' ||
        memchr(text, '\0', length)) {
        return fail(ctx, "expected uint%lld-compatible hex value", (long long)width);
    }

    int count = hex_decode(text, bytes, sizeof(bytes));
    if (count <= 0) {
        return fail(ctx, "expected uint%lld-compatible hex value", (long long)width);
    }

    // Leading zero bytes are allowed, but bytes outside the declared width
    // cannot contain significant bits. All supported widths are byte-aligned.
    for (int i = 0; i < count - width / 8; ++i) {
        if (bytes[i] != 0) {
            return fail(ctx, "value exceeds uint%lld width", (long long)width);
        }
    }

    *out = abi_uint_be(bytes, (size_t)count);
    return 1;
}

static int build_integer(
    lua_State *L,
    int descriptor_index,
    int value_index,
    lua_Integer kind,
    build_context *ctx,
    abi_value *out)
{
    lua_Integer width;
    if (!integer_field(L, descriptor_index, "bits", &width) ||
        width < 8 || width > 256 || width % 8 != 0) {
        return fail(ctx, "invalid integer descriptor width");
    }

    if (kind == LUA_ABI_UINT && lua_type(L, value_index) == LUA_TSTRING) {
        return build_uint_hex(L, value_index, width, ctx, out);
    }

    // Signed values currently accept Lua integers only, not hex strings.
    if (!lua_isinteger(L, value_index)) {
        return fail(ctx, "expected %s%lld-compatible Lua integer",
            kind == LUA_ABI_UINT ? "uint" : "int", (long long)width);
    }

    lua_Integer value = lua_tointeger(L, value_index);

    if (kind == LUA_ABI_UINT) {
        if (value < 0) {
            return fail(ctx, "negative value for uint%lld", (long long)width);
        }
        if (width < 64 && (uint64_t)value >= (UINT64_C(1) << width)) {
            return fail(ctx, "value exceeds uint%lld width", (long long)width);
        }
        *out = abi_uint64((uint64_t)value);
    } else {
        if (width < 64) {
            int64_t limit = INT64_C(1) << (width - 1);
            if (value < -limit || value >= limit) {
                return fail(ctx, "value exceeds int%lld width", (long long)width);
            }
        }
        *out = abi_int64((int64_t)value);
    }

    return 1;
}

static int build_bytes(
    lua_State *L,
    int descriptor_index,
    int value_index,
    lua_Integer kind,
    build_context *ctx,
    abi_value *out)
{
    if (lua_type(L, value_index) != LUA_TSTRING) {
        return fail(ctx, "expected raw Lua string");
    }

    size_t length;
    const char *bytes = lua_tolstring(L, value_index, &length);

    if (kind == LUA_ABI_FIXED_BYTES) {
        lua_Integer width;
        if (!integer_field(L, descriptor_index, "size", &width) ||
            width < 1 || width > 32) {
            return fail(ctx, "invalid bytesN descriptor size");
        }
        if (length != (size_t)width) {
            return fail(ctx, "expected exactly %lld raw bytes", (long long)width);
        }
        *out = abi_fixed_bytes(bytes, length);
    } else if (kind == LUA_ABI_BYTES) {
        *out = abi_bytes(bytes, length);
    } else {
        *out = abi_string_n(bytes, length);
    }

    return 1;
}

static int build_value(
    lua_State *L,
    int descriptor_index,
    int value_index,
    build_context *ctx,
    abi_value *out,
    unsigned depth);

// Called with absolute indices. build_value restores the stack and argument
// path, including on failure, so this helper can return directly from any check.
static int build_container(
    lua_State *L,
    int descriptor_index,
    int value_index,
    lua_Integer kind,
    build_context *ctx,
    abi_value *out,
    unsigned depth)
{
    size_t count;
    if (!dense_count(L, value_index, &count, ctx)) {
        return 0;
    }

    int is_tuple = kind == LUA_ABI_TUPLE;
    int element_dynamic = 0;
    raw_field(L, descriptor_index, is_tuple ? "components" : "element");
    int children_descriptor = lua_gettop(L);

    if (is_tuple) {
        size_t component_count;
        if (!dense_count(L, children_descriptor, &component_count, ctx)) {
            return 0;
        }
        if (count != component_count) {
            return fail(ctx, "tuple length mismatch: expected %zu values", component_count);
        }
    } else {
        if (!lua_istable(L, children_descriptor) ||
            !dynamic_field(L, children_descriptor, &element_dynamic)) {
            return fail(ctx, "invalid element descriptor");
        }

        if (kind == LUA_ABI_FIXED_ARRAY) {
            lua_Integer length;
            if (!integer_field(L, descriptor_index, "length", &length) || length < 0) {
                return fail(ctx, "invalid fixed-array length");
            }
            if ((lua_Unsigned)length != count) {
                return fail(ctx, "fixed-array length mismatch: expected %lld values",
                    (long long)length);
            }
        }
    }

    abi_value *children;
    if (!arena_take(ctx, count, &children)) {
        return 0;
    }

    size_t path_length = strlen(ctx->path);
    size_t path_capacity = sizeof(ctx->path) - path_length;

    for (size_t i = 0; i < count; ++i) {
        int length = snprintf(
            ctx->path + path_length, path_capacity,
            is_tuple ? ".tuple[%zu]" : "[%zu]", i + 1);

        if (length < 0 || (size_t)length >= path_capacity) {
            return fail(ctx, "argument path is too long");
        }

        // Arrays reuse one descriptor; tuples select a descriptor per member.
        int child_descriptor = children_descriptor;
        if (is_tuple) {
            lua_rawgeti(L, children_descriptor, (lua_Integer)i + 1);
            child_descriptor = lua_gettop(L);
        }
        lua_rawgeti(L, value_index, (lua_Integer)i + 1);

        abi_value *child = children ? children + i : NULL;
        if (!build_value(L, child_descriptor, -1, ctx, child, depth + 1)) {
            return 0;
        }

        lua_pop(L, is_tuple ? 2 : 1);
        ctx->path[path_length] = '\0';
    }

    if (out) {
        if (kind == LUA_ABI_ARRAY) {
            *out = abi_array(children, count);
        } else if (is_tuple) {
            *out = abi_tuple(children, count);
        } else if (count == 0 && element_dynamic) {
            // With no children, abi.c cannot infer the dynamic element type.
            *out = abi_fixed_array_dynamic(NULL, 0);
        } else {
            *out = abi_fixed_array(children, count);
        }
    }

    return 1;
}

// Each recursive visit owns its stack and path cleanup. Type-specific helpers
// only validate and construct values, leaving cleanup to this common boundary.
static int build_value(
    lua_State *L,
    int descriptor_index,
    int value_index,
    build_context *ctx,
    abi_value *out,
    unsigned depth)
{
    descriptor_index = lua_absindex(L, descriptor_index);
    value_index = lua_absindex(L, value_index);

    int top = lua_gettop(L);
    int ok = 0;
    size_t path_length = strlen(ctx->path);
    abi_value result = {0};
    lua_Integer kind;

    if (depth > ABI_MAX_DEPTH) {
        fail(ctx, "maximum ABI nesting depth exceeded");
        goto cleanup;
    }
    if (!lua_checkstack(L, 8)) {
        fail(ctx, "cannot grow Lua stack");
        goto cleanup;
    }
    if (!lua_istable(L, descriptor_index)) {
        fail(ctx, "expected a compiled descriptor");
        goto cleanup;
    }
    if (!integer_field(L, descriptor_index, "kind", &kind)) {
        fail(ctx, "invalid descriptor kind");
        goto cleanup;
    }

    switch (kind) {
    case LUA_ABI_BOOL:
        if (!lua_isboolean(L, value_index)) {
            fail(ctx, "expected boolean");
            goto cleanup;
        }
        result = abi_bool(lua_toboolean(L, value_index));
        ok = 1;
        break;

    case LUA_ABI_ADDRESS:
        ok = build_address(L, value_index, ctx, &result);
        break;

    case LUA_ABI_UINT:
    case LUA_ABI_INT:
        ok = build_integer(L, descriptor_index, value_index, kind, ctx, &result);
        break;

    case LUA_ABI_FIXED_BYTES:
    case LUA_ABI_BYTES:
    case LUA_ABI_STRING:
        ok = build_bytes(L, descriptor_index, value_index, kind, ctx, &result);
        break;

    case LUA_ABI_ARRAY:
    case LUA_ABI_FIXED_ARRAY:
    case LUA_ABI_TUPLE:
        ok = build_container(
            L, descriptor_index, value_index, kind, ctx,
            out ? &result : NULL, depth);
        break;

    default:
        fail(ctx, "unknown ABI kind: %lld", (long long)kind);
        break;
    }

    if (ok && out) {
        *out = result;
    }

cleanup:
    ctx->path[path_length] = '\0';
    lua_settop(L, top);
    return ok;
}

// Reserve the selector and root arguments before allocating nested children.
// Indices are absolute; the public entry point restores the stack on failure.
static int build_arguments(
    lua_State *L,
    int params_index,
    int values_index,
    size_t count,
    const uint8_t selector[4],
    build_context *ctx,
    abi_value **root)
{
    if (!arena_take(ctx, count + 1, root)) {
        return 0;
    }
    if (*root) {
        (*root)[0] = abi_selector(selector);
    }

    for (size_t i = 0; i < count; ++i) {
        snprintf(ctx->path, sizeof(ctx->path), "argument #%zu", i + 1);
        lua_rawgeti(L, params_index, (lua_Integer)i + 1);
        lua_rawgeti(L, values_index, (lua_Integer)i + 1);

        abi_value *argument = *root ? *root + i + 1 : NULL;
        if (!build_value(L, -2, -1, ctx, argument, 1)) {
            return 0;
        }
        lua_pop(L, 2);
    }

    ctx->path[0] = '\0';
    return 1;
}

int lua_abi_encode_call(
    lua_State *L,
    int call_index,
    abi_buffer *out,
    char *error,
    size_t error_capacity)
{
    build_context ctx = {
        .arena = { .capacity = SIZE_MAX / sizeof(abi_value) },
        .error = error,
        .error_capacity = error_capacity
    };

    if (error && error_capacity) {
        error[0] = '\0';
    }
    if (!out) {
        return fail(&ctx, "missing output buffer");
    }
    *out = (abi_buffer){0};

    if (!L) {
        return fail(&ctx, "missing Lua state");
    }

    call_index = lua_absindex(L, call_index);
    int top = lua_gettop(L);
    int ok = 0;

    if (!lua_checkstack(L, 8)) {
        return fail(&ctx, "cannot grow Lua stack");
    }
    if (!lua_istable(L, call_index)) {
        return fail(&ctx, "expected a Call table");
    }

    // Keep these fields on the stack through both passes and final encoding.
    raw_field(L, call_index, "selector");
    size_t selector_length;
    const char *selector = lua_type(L, -1) == LUA_TSTRING
        ? lua_tolstring(L, -1, &selector_length) : NULL;

    if (!selector || selector_length != 4) {
        fail(&ctx, "Call.selector must contain exactly 4 raw bytes");
        goto cleanup;
    }

    raw_field(L, call_index, "params");
    int params_index = lua_gettop(L);
    raw_field(L, call_index, "values");
    int values_index = lua_gettop(L);
    size_t param_count;
    size_t value_count;

    if (!dense_count(L, params_index, &param_count, &ctx) ||
        !dense_count(L, values_index, &value_count, &ctx)) {
        goto cleanup;
    }
    if (param_count != value_count) {
        fail(&ctx, "Call.params and Call.values counts differ");
        goto cleanup;
    }

    // First pass: count slots without allocating any descriptor storage.
    abi_value *root;
    if (!build_arguments(L, params_index, values_index, param_count,
            (const uint8_t *)selector, &ctx, &root)) {
        goto cleanup;
    }

    ctx.arena.capacity = ctx.arena.used;
    ctx.arena.values = malloc(ctx.arena.capacity * sizeof(abi_value));
    if (!ctx.arena.values) {
        fail(&ctx, "cannot allocate ABI descriptor storage");
        goto cleanup;
    }
    ctx.arena.used = 0;

    // Second pass: populate the arena, then delegate byte layout to abi.c.
    if (!build_arguments(L, params_index, values_index, param_count,
            (const uint8_t *)selector, &ctx, &root)) {
        goto cleanup;
    }

    abi_status status = abi_encode_alloc(root, param_count + 1, out);
    if (status != ABI_OK) {
        fail(&ctx, "ABI encoding failed: %s", abi_status_string(status));
        goto cleanup;
    }

    ok = 1;

cleanup:
    free(ctx.arena.values);
    lua_settop(L, top);
    return ok;
}
