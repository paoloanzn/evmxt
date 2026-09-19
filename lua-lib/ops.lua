--[[
    This module defines (1) the list of supported operations
    that can be encoded in Lua and ran by the C engine, and
    (2) the Op object.

    The Op object contains the operation's name, dispatch index, and
    operation's raw data, and an optional destination for calls.

    Usage: wrap a Call in an operation:
    local Call = require("lua-lib.call").Call
    local Op = require("lua-lib.ops").Op
    local batch = Call.compile("batch", { "(bool,uint256[])[]" })
    local op = Op.new("call", batch({ { true, { 1, 2 } }, { false, { 3 } } }), token)
    local wallet = Op.new("wallet_create")
]]--

local M = {}
local Call = require("lua-lib.call").Call

-- Each definition contains { operation name, payload class or nil, index }.
-- Indices are zero-based and must match runtime.c's op enum and dispatch table.
-- Add new operations with the next index and a corresponding C handler.
---@enum operation
local operation = {
    call = { "call", Call, 0 },
    wallet_create = { "wallet_create", nil, 1 },
    get_chain_id = { "get_chain_id", nil, 2 }
}

---@class Op
---@field type string
---@field index integer Zero-based C dispatch index.
---@field data any
---@field to string|nil Destination address for call operations.
local Op = {}
Op.__index = Op

---@param op string
---@return boolean
local function isValidOpType(op)
    return type(op) == "string" and operation[op] ~= nil
end

---@param op string
---@param data any
---@return boolean
local function isValidOpData(op, data)
    local payloadClass = operation[op][2]
    if payloadClass == nil then
        return data == nil
    end
    return type(data) == "table" and getmetatable(data) == payloadClass
end

-- Runtime validation for Op objects
---@param op string
---@param data any
---@param to? string
---@return Op
function Op.new(op, data, to)
    assert(isValidOpType(op), "Invalid operation: " .. tostring(op))
    assert(isValidOpData(op, data), "Invalid data for operation: " .. op)
    assert(to == nil or op == "call", "Only call operations accept a destination")
    assert(to == nil or (type(to) == "string" and #to == 42
        and to:match("^0x[0-9a-fA-F]+$")), "Invalid destination address")
    return setmetatable({
        type = op,
        index = operation[op][3],
        data = data,
        to = to
    }, Op)
end

M.Op = Op

return M
