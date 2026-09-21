--[[
    This module defines Tx, a transaction ready to be sent. It holds two
    things: the contract address to send to, and a Call describing which
    function to run and what values to give it.

    Tx.new checks the address and makes sure the call is a Call object.
    It only builds the transaction; use evm.send(tx) to send it later.
    Treat the transaction and its Call as read-only once they are created.

    Usage: give a prepared Call a destination:

    local Tx = require("lua-lib.tx").Tx
    local tx = Tx.new({ to = token_address, call = transfer(recipient, amount) })
]]--

local M = {}
local Call = require("lua-lib.call").Call
local address = require("lua-lib.address")

---@class TxArgs
---@field to string Contract address: 0x followed by 40 hex digits.
---@field call Call Prepared function call.

---@class Tx
---@field to string
---@field call Call
local Tx = {}
Tx.__index = Tx

-- Treat the transaction and its Call as read-only after construction.
---@param args TxArgs
---@return Tx
function Tx.new(args)
    assert(type(args) == "table", "Tx arguments must be a table")
    address.validate(args.to, "Tx.to")
    assert(type(args.call) == "table" and getmetatable(args.call) == Call,
        "Tx.call must be a Call")
    return setmetatable({ to = args.to, call = args.call }, Tx)
end

M.Tx = Tx

return M
