--[[
    This module defines (1) the list of supported operations
    that can be encoded in Lua and ran by the C engine, and
    (2) the Op object.

    The Op object contains the operation's name and
    operation's raw data.

    Usage: wrap a Call in an operation:
    local Call = require("configuration.call").Call
    local Op = require("configuration.ops").Op
    local op = Op.new("call", Call.new("batch", {
        { type = "(bool,uint256[])[]", value = { { true, { 1, 2 } }, { false, { 3 } } } }
    }))
    local wallet = Op.new("wallet_create")
]]--

local M = {}
local Call = require("configuration.call").Call

-- Each definition contains { operation name, payload class or nil }.
---@enum operation
local operation = {
    call = { "call", Call },
    wallet_create = { "wallet_create", nil }
}

---@class Op
---@field type string
---@field data any
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

---Runtime validation for Op objects
---@param type string
---@param data any
---@return Op
function Op.new(type, data)
    assert(isValidOpType(type), "Invalid operation: " .. tostring(type))
    assert(isValidOpData(type, data), "Invalid data for operation: " .. type)
    return setmetatable({
        type = type,
        data = data
    }, Op)
end

M.Op = Op

return M
