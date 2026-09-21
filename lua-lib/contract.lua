--[[
    This module defines Contract, which keeps a contract address together
    with the functions you want to call on it.

    Contract.new prepares each function once using its name and argument
    types. Later, contract:tx supplies the values and returns a Tx with the
    saved address. The same prepared function can be used again and again.

    Building a Tx does not send it. Use evm.send(tx) when you want to send it.

    Usage: prepare a transfer function, then build a transaction:

    local Contract = require("lua-lib.contract").Contract
    local token = Contract.new({
        address = token_address,
        methods = { transfer = { "address", "uint256" } },
    })
    local tx = token:tx("transfer", recipient, amount)
]]--

local M = {}
local Call = require("lua-lib.call").Call
local Tx = require("lua-lib.tx").Tx
local address = require("lua-lib.address")

---@class ContractArgs
---@field address string Contract address: 0x followed by 40 hex digits.
---@field methods table<string, string[]> Function names mapped to ABI argument types.

---@class CompiledContractMethod
---@overload fun(...: any): Call

---@class Contract
---@field address string
---@field private _methods table<string, CompiledContractMethod> Prepared functions reused by tx().
local Contract = {}
Contract.__index = Contract

---@param args ContractArgs
---@return Contract
function Contract.new(args)
    assert(type(args) == "table", "Contract arguments must be a table")
    address.validate(args.address, "Contract.address")
    assert(type(args.methods) == "table", "Contract.methods must be a table")
    ---@type table<string, CompiledContractMethod>
    local methods = {}
    for name, types in pairs(args.methods) do
        methods[name] = Call.compile(name, types)
    end
    return setmetatable({ address = args.address, _methods = methods }, Contract)
end

---@param method string Name of a function declared at construction.
---@param ... any Positional ABI values; pass each tuple or array as one table.
---@return Tx
function Contract:tx(method, ...)
    local compiled = self._methods[method]
    assert(compiled, "unknown contract method: " .. tostring(method))
    return Tx.new({ to = self.address, call = compiled(...) })
end

M.Contract = Contract

return M
