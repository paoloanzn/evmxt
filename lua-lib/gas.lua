--[[
    This module prepares gas settings for the C runtime by checking the limit
    and fees when a Gas object is created, then storing each fee as 32 bytes
    so the operation can copy it directly into the transaction context.

    The gas limit must be a nonnegative Lua integer, while fees are amounts
    in wei supplied as nonnegative integers or 0x hex strings up to 32 bytes,
    and the priority fee must not be greater than the maximum fee.

    Usage: create the settings and pass them to the set_gas operation, treating
    the resulting object as read-only so its validated values stay consistent:

    local Gas = require("lua-lib.gas").Gas
    local gas = Gas.new({
        gas_limit = 21000,
        max_priority_fee_per_gas = 1000000000,
        max_fee_per_gas = 30000000000
    })
    local op = Op.new("set_gas", gas)
]]--

local M = {}

---@class Gas
---@field gas_limit integer
---@field max_priority_fee_per_gas string 32 raw big-endian bytes, in wei.
---@field max_fee_per_gas string 32 raw big-endian bytes, in wei.
local Gas = {}
Gas.__index = Gas

local function fee(value, name)
    if math.type(value) == "integer" then
        assert(value >= 0, name .. " must be nonnegative")
        return string.rep("\0", 24) .. string.pack(">I8", value)
    end

    assert(type(value) == "string" and #value >= 3 and #value <= 66
        and value:match("^0x[0-9a-fA-F]+$"),
        name .. " must be a nonnegative integer or 0x hex value up to 32 bytes")

    local hex = string.rep("0", 66 - #value) .. value:sub(3)
    return (hex:gsub("..", function(pair)
        return string.char(tonumber(pair, 16))
    end))
end

---@param settings table
---@return Gas
function Gas.new(settings)
    assert(type(settings) == "table", "Gas settings must be a table")
    local limit = settings.gas_limit
    assert(math.type(limit) == "integer" and limit >= 0,
        "gas_limit must be a nonnegative Lua integer")

    local priority = fee(settings.max_priority_fee_per_gas, "max_priority_fee_per_gas")
    local maximum = fee(settings.max_fee_per_gas, "max_fee_per_gas")

    -- Compare from the most significant byte without converting to a Lua number.
    for i = 1, 32 do
        local a, b = priority:byte(i), maximum:byte(i)
        if a ~= b then
            assert(a < b, "max_priority_fee_per_gas must not exceed max_fee_per_gas")
            break
        end
    end

    return setmetatable({
        gas_limit = limit,
        max_priority_fee_per_gas = priority,
        max_fee_per_gas = maximum,
    }, Gas)
end

M.Gas = Gas

return M
