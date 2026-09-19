--[[
    This module defines the Call interface, which prepares the function
    description and argument values that the C engine needs for ABI encoding,
    while leaving value validation, encoding and transaction execution to C.

    Call.compile takes a function name and its parameter types and returns a
    reusable definition that can be prepared at program startup, so each later
    call only supplies positional values without parsing types or computing
    the selector again.

    Array and tuple arguments are passed directly as Lua tables, and both
    these values and the compiled descriptors are shared with the resulting
    Call objects, so they should be treated as read-only while in use.

    The destination belongs to the surrounding Op, while transaction settings
    such as gas limit, nonce and priority fee are handled in a separate workflow.

    Usage: compile a transfer once, then supply its arguments and destination:

    local Call = require("lua-lib.call").Call
    local Op = require("lua-lib.ops").Op
    local transfer = Call.compile("transfer", { "address", "uint256" })
    local op = Op.new("call", transfer(recipient, amount), token)
]]--

local M = {}
local compiler = require("lua-lib.compiler")

---@class Call
---@field selector string Four raw bytes.
---@field signature string Canonical function signature.
---@field params table[] Shared compiled type descriptors.
---@field values table Positional argument values.
local Call = {}
Call.__index = Call

local CompiledCall = {}

function CompiledCall:__call(...)
    local count = #self.params
    assert(select("#", ...) == count, "Incorrect argument count")
    local values = { ... }
    for i = 1, count do
        assert(values[i] ~= nil, "Missing argument #" .. i)
    end

    return setmetatable({
        selector = self.selector,
        signature = self.signature,
        params = self.params,
        values = values,
    }, Call)
end

-- Compile a reusable, callable definition without values or a destination.
---@param name string
---@param types string[]
---@return table
function Call.compile(name, types)
    return setmetatable(compiler.compile(name, types), CompiledCall)
end

M.Call = Call
M.ABI = compiler.ABI

return M
