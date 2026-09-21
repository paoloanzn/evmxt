#include <lauxlib.h>
#include <openssl/bn.h>
#include <stdint.h>

enum parse_result {
    PARSE_OK,
    PARSE_INVALID,
    PARSE_ZERO,
    PARSE_ALLOCATION_FAILED
};

// OpenSSL keeps temporary big numbers in a BN_CTX. One context belongs to the
// exported Lua function, so repeated price calculations can reuse its memory.
typedef struct {
    BN_CTX *bn;
} sqrt_context;

// Read the same unsigned values accepted by the Lua ABI layer: a positive Lua
// integer, or a 0x-prefixed string containing at most 256 bits.
static enum parse_result parse_positive_integer(lua_State *L, int index, BIGNUM *result)
{
    if (lua_isinteger(L, index)) {
        lua_Integer value = lua_tointeger(L, index);
        if (value <= 0) {
            return PARSE_ZERO;
        }

        // BN_bin2bn reads big-endian bytes. Building the byte array explicitly
        // also works when OpenSSL's native word is smaller than lua_Integer.
        uint64_t integer = (uint64_t)value;
        unsigned char bytes[sizeof(integer)];
        for (size_t i = 0; i < sizeof(bytes); ++i) {
            bytes[sizeof(bytes) - i - 1] = (unsigned char)(integer >> (i * 8));
        }
        return BN_bin2bn(bytes, sizeof(bytes), result)
            ? PARSE_OK : PARSE_ALLOCATION_FAILED;
    }

    size_t length = 0;
    const char *hex = lua_tolstring(L, index, &length);

    // Two prefix bytes plus 64 hex digits is one uint256. Validate every byte
    // before handing the string to OpenSSL so embedded data is never ignored.
    if (!hex || length < 3 || length > 66 || hex[0] != '0' || hex[1] != 'x') {
        return PARSE_INVALID;
    }
    for (size_t i = 2; i < length; ++i) {
        char digit = hex[i];
        if (!((digit >= '0' && digit <= '9')
                || (digit >= 'a' && digit <= 'f')
                || (digit >= 'A' && digit <= 'F'))) {
            return PARSE_INVALID;
        }
    }

    // Supplying an existing BIGNUM lets BN_hex2bn reuse storage from BN_CTX.
    BIGNUM *target = result;
    if (BN_hex2bn(&target, hex + 2) != (int)(length - 2)) {
        return PARSE_ALLOCATION_FAILED;
    }
    return BN_is_zero(result) ? PARSE_ZERO : PARSE_OK;
}

// Newton iteration starts above sqrt(value) and converges quadratically to
// floor(sqrt(value)). Shifts replace division by two in the update step.
static inline int integer_sqrt(BIGNUM *root, const BIGNUM *value, BN_CTX *ctx)
{
    BIGNUM *quotient = BN_CTX_get(ctx);
    BIGNUM *next = BN_CTX_get(ctx);
    if (!next) {
        return 0;
    }

    // A number with n significant bits has a square root no larger than
    // 2^ceil(n/2). Starting above the answer makes every iteration decrease.
    int initial_bit = (BN_num_bits(value) + 1) >> 1;
    BN_zero(root);
    if (!BN_set_bit(root, initial_bit)) {
        return 0;
    }

    for (;;) {
        // next = floor((root + floor(value / root)) / 2)
        if (!BN_div(quotient, NULL, value, root, ctx)
                || !BN_add(next, root, quotient)
                || !BN_rshift1(next, next)) {
            return 0;
        }

        // Integer Newton iteration has settled once rounding would keep the
        // guess unchanged or move it upward. The current guess is then floor.
        if (BN_cmp(next, root) >= 0) {
            return 1;
        }
        if (!BN_copy(root, next)) {
            return 0;
        }
    }
}

// Lua's ABI encoder already accepts hexadecimal strings for values wider than
// a signed Lua integer. Emit the shortest lowercase representation it accepts.
static int push_uint160_hex(lua_State *L, const BIGNUM *value)
{
    static const char digits[] = "0123456789abcdef";
    unsigned char bytes[20];
    char encoded[42] = "0x";

    if (BN_bn2binpad(value, bytes, sizeof(bytes)) != (int)sizeof(bytes)) {
        return 0;
    }

    // The caller has already rejected zero, so a nonzero byte always exists.
    size_t first = 0;
    while (bytes[first] == 0) {
        ++first;
    }

    size_t output = 2;

    // Avoid a leading zero nibble while keeping two digits for later bytes.
    if (bytes[first] < 16) {
        encoded[output++] = digits[bytes[first++]];
    }
    for (size_t i = first; i < sizeof(bytes); ++i) {
        encoded[output++] = digits[bytes[i] >> 4];
        encoded[output++] = digits[bytes[i] & 15];
    }
    lua_pushlstring(L, encoded, output);
    return 1;
}

static int sqrt_price_limit_x96(lua_State *L)
{
    // The userdata captured by this C closure owns the reusable OpenSSL context.
    sqrt_context *native = lua_touserdata(L, lua_upvalueindex(1));
    BN_CTX *ctx = native->bn;

    // BN_CTX_start/end makes all temporary BIGNUMs below behave like one stack
    // frame. Ending the frame keeps their storage available for the next call.
    BN_CTX_start(ctx);

    BIGNUM *amount1 = BN_CTX_get(ctx);
    BIGNUM *amount0 = BN_CTX_get(ctx);
    BIGNUM *root = BN_CTX_get(ctx);
    if (!root) {
        BN_CTX_end(ctx);
        return luaL_error(L, "Unable to allocate sqrtPriceLimitX96 operands");
    }

    // Parse both arguments before doing arithmetic so failures name the input
    // that the Lua caller needs to correct.
    enum parse_result first = parse_positive_integer(L, 1, amount1);
    enum parse_result second = parse_positive_integer(L, 2, amount0);

    if (first != PARSE_OK || second != PARSE_OK) {
        BN_CTX_end(ctx);
        if (first == PARSE_ALLOCATION_FAILED || second == PARSE_ALLOCATION_FAILED) {
            return luaL_error(L, "Unable to allocate sqrtPriceLimitX96 operands");
        }
        const char *name = first != PARSE_OK ? "amount1" : "amount0";
        enum parse_result failure = first != PARSE_OK ? first : second;
        return luaL_error(L, failure == PARSE_ZERO
            ? "%s must be positive"
            : "%s must be a positive integer or 0x hex string", name);
    }

    // Uniswap stores sqrt(token1 / token0) as a Q64.96 number:
    //
    //   floor(sqrt(amount1 / amount0) * 2^96)
    // = floor(sqrt((amount1 << 192) / amount0))
    //
    // Taking the integer quotient first does not change the final floor.
    BIGNUM *scaled = BN_CTX_get(ctx);
    BIGNUM *quotient = BN_CTX_get(ctx);
    int ok = quotient
        && BN_lshift(scaled, amount1, 192)
        && BN_div(quotient, NULL, scaled, amount0, ctx);
    int overflow = 0;
    int underflow = 0;

    // A uint160 square root cannot come from a quotient wider than 320 bits.
    // Checking before Newton avoids unnecessary divisions for invalid inputs.
    underflow = ok && BN_is_zero(quotient);
    overflow = ok && BN_num_bits(quotient) > 320;
    ok = ok && !underflow && !overflow && integer_sqrt(root, quotient, ctx);

    if (ok && BN_num_bits(root) > 160) {
        overflow = 1;
        ok = 0;
    }
    if (ok) {
        // Return the value in the same form expected by a uint160 Call argument.
        ok = push_uint160_hex(L, root);
    }

    BN_CTX_end(ctx);

    if (ok) {
        return 1;
    }
    if (underflow) {
        return luaL_error(L, "sqrtPriceLimitX96 underflows uint160");
    }
    if (overflow) {
        return luaL_error(L, "sqrtPriceLimitX96 exceeds uint160");
    }
    return luaL_error(L, "Unable to compute sqrtPriceLimitX96");
}

// The closure keeps its userdata alive. Lua calls this finalizer after the
// exported function becomes unreachable, which releases the shared BN_CTX.
static int free_sqrt_context(lua_State *L)
{
    sqrt_context *native = lua_touserdata(L, 1);
    BN_CTX_free(native->bn);
    native->bn = NULL;
    return 0;
}

int luaopen_uniswap_v3_native(lua_State *L)
{
    lua_newtable(L);
    // lua_stack = [module]

    // Capture the context as userdata instead of a process-global value. This
    // keeps separate Lua states independent and gives the context a clear owner.
    sqrt_context *native = lua_newuserdatauv(L, sizeof(*native), 0);
    native->bn = BN_CTX_new();
    if (!native->bn) {
        return luaL_error(L, "Unable to allocate Uniswap V3 math context");
    }

    if (luaL_newmetatable(L, "evmxt.uniswap_v3_native")) {
        lua_pushcfunction(L, free_sqrt_context);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);

    // lua_stack = [module, context]
    // lua_pushcclosure consumes the context and stores it as upvalue #1.
    lua_pushcclosure(L, sqrt_price_limit_x96, 1);
    lua_setfield(L, -2, "sqrtPriceLimitX96");
    return 1;
}
