local Contract = require("lua-lib.contract").Contract
local address = require("lua-lib.address")

---@class UniswapV3Args
---@field router string Address of a router supporting the seven-field exactInputSingle ABI.

---@class UniswapV3SwapArgs
---@field tokenIn string Input token address.
---@field tokenOut string Output token address.
---@field amountIn integer|string Input amount in token base units: nonnegative integer or 0x hex string.
---@field amountOutMinimum integer|string Minimum output in token base units: nonnegative integer or 0x hex string.
---@field recipient string Address receiving the output tokens.
---@field fee integer Pool fee, from 0 through 16777215 (uint24).
---@field sqrtPriceLimitX96? integer|string uint160 price limit: nonnegative integer or 0x hex string; defaults to 0.

---@class UniswapV3
---@field router Contract
local UniswapV3 = {}
UniswapV3.__index = UniswapV3

-- Configure a router supporting the seven-field, no-deadline ABI below.
---@param args UniswapV3Args
---@return UniswapV3
function UniswapV3.new(args)
    assert(type(args) == "table", "UniswapV3 arguments must be a table")
    return setmetatable({ router = Contract.new({
        address = args.router,
        methods = {
            exactInputSingle = {
                "(address,address,uint24,address,uint256,uint256,uint160)",
            },
        },
    }) }, UniswapV3)
end

---@param args UniswapV3SwapArgs
---@return Tx
function UniswapV3:swap(args)
    assert(type(args) == "table", "swap arguments must be a table")
    for _, name in ipairs({
        "tokenIn", "tokenOut", "amountIn", "amountOutMinimum", "recipient", "fee",
    }) do
        assert(args[name] ~= nil, "missing swap argument: " .. name)
    end
    for _, name in ipairs({ "tokenIn", "tokenOut", "recipient" }) do
        address.validate(args[name], name)
    end
    assert(math.type(args.fee) == "integer" and args.fee >= 0 and args.fee <= 0xffffff,
        "fee must be a nonnegative Lua integer within uint24 bounds")
    local limit = args.sqrtPriceLimitX96
    if limit == nil then limit = 0 end
    return self.router:tx("exactInputSingle", {
        args.tokenIn, args.tokenOut, args.fee, args.recipient,
        args.amountIn, args.amountOutMinimum, limit,
    })
end

return UniswapV3
