--[[
    This module defines a Call object interface.
    The Call object is the only data structure,
    related to transactions, that the C engine executes.

    Everything in transactions chain from ABI encoding and on,
    belongs to the C engine, not to Lua. Lua stops just before
    that. So a Call object represents what C needs to perform
    ABI encoding.

    All the other transactions-related parameters such as
    gas_limit, nonce, priority_fee and others, do not belong
    here. They are instead handled in a separate workflow.


    Usage: compose a transaction's Call using positional tuple/array values:
    local Call = require("lua-lib.call").Call
    local call = Call.new("batch", {
        { type = "(bool,uint256[])[]", value = { { true, { 1, 2 } }, { false, { 3 } } } }
    }, "0x1111111111111111111111111111111111111111")
    -- Omit the third argument (or pass nil) when no destination is needed.
]]--

local M = {}

---@class AbiParam
---@field type string ABI type, e.g. uint256, (address,uint256[]), (bool,bytes)[2][]
---@field value any

---@class Call
---@field name string
---@field params AbiParam[]
---@field to string|nil Destination address: 0x followed by 40 hex digits (no checksum validation).
local Call = {}
Call.__index = Call

local function isValidAbiType(t, depth)
    depth = (depth or 0) + 1
    if type(t) ~= "string" or depth > 64 then return false end
    local base, size = t:match("^(.-)%[([0-9]*)%]$")
    if base then
        return (size == "" or size == "0" or size:match("^[1-9][0-9]*$") ~= nil)
            and isValidAbiType(base, depth)
    end
    if t:sub(1, 1) == "(" and t:sub(-1) == ")" then
        local inner, level, start = t:sub(2, -2), 0, 1
        if inner == "" then return true end
        for i = 1, #inner do
            local c = inner:sub(i, i)
            if c == "(" then level = level + 1 end
            if c == ")" then level = level - 1 end
            if level < 0 then return false end
            if c == "," and level == 0 then
                if not isValidAbiType(inner:sub(start, i - 1), depth) then return false end
                start = i + 1
            end
        end
        return level == 0 and isValidAbiType(inner:sub(start), depth)
    end
    if t == "address" then
        return true
    end

    if t == "bool" then
        return true
    end

    if t == "string" then
        return true
    end

    if t == "bytes" then
        return true
    end

    if t:match("^uint%d*$") then
        local bits = t == "uint" and 256 or tonumber(t:match("^uint([1-9]%d*)$"))
        return bits ~= nil and bits <= 256 and bits % 8 == 0
    end

    if t:match("^int%d*$") then
        local bits = t == "int" and 256 or tonumber(t:match("^int([1-9]%d*)$"))
        return bits ~= nil and bits <= 256 and bits % 8 == 0
    end

    if t:match("^bytes%d+$") then
        local size = tonumber(t:match("^bytes([1-9]%d*)$"))
        return size ~= nil and size <= 32
    end

    return false
end

-- Runtime validation for Call objects 
---@param name string
---@param params AbiParam[]
---@param to? string Destination address: 0x followed by 40 hex digits.
---@return Call
function Call.new(name, params, to)
    assert(to == nil or (type(to) == "string" and #to == 42 and to:match("^0x[0-9a-fA-F]+$")),
        "Invalid destination address: expected nil or 0x followed by 40 hex digits")
    assert(type(name) == "string")
    assert(name:match("^[%a_][%w_]*$"), "Invalid function name")
    assert(type(params) == "table", "Parameters must be an array")
    local count = 0
    for key in pairs(params) do
        count = count + 1
        assert(type(key) == "number" and key % 1 == 0 and key >= 1, "Invalid parameter index")
    end
    for i = 1, count do assert(rawget(params, i) ~= nil, "Sparse parameters") end
    
        for i, param in ipairs(params) do
            assert(type(param) == "table", "Invalid parameter #" .. i)
            assert(
                isValidAbiType(param.type),
                "Invalid ABI type: " .. tostring(param.type)
            )
        end

        return setmetatable({
            name = name,
            params = params,
            to = to
        }, Call)
end


M.Call = Call
return M
