--[[
    Build an exact-input swap call for a Uniswap V3 SwapRouter02 contract.

    Usage:

    local UniswapV3 = require("plugins.uniswap_v3.uniswap_v3")
    local Chain = require("lua-lib.chains").BASE
    local router = require("plugins.uniswap_v3.deployments")[Chain].router
    local Op = require("lua-lib.ops").Op
    local sqrtPriceLimitX96 = UniswapV3.sqrtPriceLimitX96(
        limitAmount1,
        limitAmount0
    )

    local swap = UniswapV3.exactInputSingle({
        tokenIn,
        tokenOut,
        UniswapV3.FEE.MEDIUM,
        recipient,
        amountIn,
        amountOutMinimum,
        sqrtPriceLimitX96,
    })

    send_op(Op.new("call", swap, router))
]]--

local Call = require("lua-lib.call").Call

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

return M
