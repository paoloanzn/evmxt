--[[
    Build Uniswap V3 swap calls for a SwapRouter02 contract.

    Usage:

    local UniswapV3 = require("plugins.uniswap_v3.uniswap_v3")
    local Chain = require("lua-lib.chains").BASE
    local deployment = require("plugins.uniswap_v3.deployments")[Chain]
    local Op = require("lua-lib.ops").Op
    local sqrtPriceLimitX96 = UniswapV3.sqrtPriceLimitX96(
        limitAmount1,
        limitAmount0
    )
    local amountOutMinimum = UniswapV3.quoteAmountOutMinimum({
        tokenIn, tokenOut, amountIn, UniswapV3.FEE.MEDIUM, sqrtPriceLimitX96
    }, 0.02, deployment.quoter)

    local swap = UniswapV3.exactInputSingle({
        tokenIn,
        tokenOut,
        UniswapV3.FEE.MEDIUM,
        recipient,
        amountIn,
        amountOutMinimum,
        sqrtPriceLimitX96,
    })

    send_op(Op.new("call", swap, deployment.router))
]]--

local Call = require("lua-lib.call").Call
local Op = require("lua-lib.ops").Op

local M = {}
local directory = assert(debug.getinfo(1, "S").source:match("^@(.*/)"))
local native = assert(package.loadlib(
    directory .. "../../lua-lib/c-bindings/bindings.so",
    "luaopen_uniswap_v3_native"
))()

---@enum UniswapV3Fee
M.FEE = {
    LOWEST = 100,
    LOW = 500,
    MEDIUM = 3000,
    HIGH = 10000,
}

-- Compute floor(sqrt(amount1 / amount0) * 2^96) in the native binding.
-- The positive token base-unit amounts may be integers or 0x hex strings.
---@type fun(amount1: integer|string, amount0: integer|string): string
M.sqrtPriceLimitX96 = native.sqrtPriceLimitX96

M.exactInputSingle = Call.compile("exactInputSingle", {
    "(address,address,uint24,address,uint256,uint256,uint160)",
})

-- {tokenIn, tokenOut, fee, recipient, amountOut, amountInMaximum, sqrtPriceLimitX96}
M.exactOutputSingle = Call.compile("exactOutputSingle", {
    "(address,address,uint24,address,uint256,uint256,uint160)",
})
M.exactInput = Call.compile("exactInput", {
    "(bytes,address,uint256,uint256)",
})
-- {path, recipient, amountOut, amountInMaximum}; path runs output to input.
M.exactOutput = Call.compile("exactOutput", {
    "(bytes,address,uint256,uint256)",
})

local quoteExactInputSingle = Call.compile("quoteExactInputSingle", {"(address,address,uint256,uint24,uint160)"})
-- params: {tokenIn, tokenOut, amountIn, fee, sqrtPriceLimitX96}.
-- Returns a 0x uint256; slippage is a fraction, rounded up to one ppm.
function M.quoteAmountOutMinimum(params, slippage, quoter)
    assert(type(slippage) == "number" and slippage >= 0 and slippage < 1,
        "slippage must be a fraction in [0, 1)")
    assert(quoter ~= nil, "QuoterV2 address required")
    local result = coroutine.yield(Op.new("eth_call", quoteExactInputSingle(params), quoter))
    assert(type(result) == "string" and #result == 258
        and result:match("^0x[0-9a-fA-F]+$"), "Invalid QuoterV2 response")
    local factor = 1000000 - math.ceil(slippage * 1000000)
    local digits, carry = {}, 0
    for i = 66, 3, -1 do
        local value = tonumber(result:sub(i, i), 16) * factor + carry
        digits[#digits + 1] = value % 16
        carry = math.floor(value / 16)
    end
    while carry > 0 do
        digits[#digits + 1] = carry % 16
        carry = math.floor(carry / 16)
    end
    local remainder, output = 0, {}
    for i = #digits, 1, -1 do
        local value = remainder * 16 + digits[i]
        output[#output + 1] = string.format("%x", math.floor(value / 1000000))
        remainder = value % 1000000
    end
    return "0x" .. (table.concat(output):gsub("^0+", ""):match(".+") or "0")
end

return M
