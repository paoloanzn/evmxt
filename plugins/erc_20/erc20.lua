--[[
    Build approve and transfer operations for an ERC-20 token.

    Usage:

    local ERC20 = require("plugins.erc_20.erc20")
    local Chain = require("lua-lib.chains").BASE
    local Token = require("lua-lib.tokens").USDC[Chain]
    local spender = "0x....."
    local recepient = "0x....."
    local amount = 500
    
    send_op(Op.new("call", approve(spender, amount * Token.decimals), Token.address))
    send_op(Op.new("call", transfer(recipient, amount * Token.decimals), Token.address))
]]--

local Call = require("lua-lib.call").Call

local M = {}

M.approve = Call.compile("approve", { "address", "uint256" })
M.transfer = Call.compile("transfer", { "address", "uint256" })

return M
