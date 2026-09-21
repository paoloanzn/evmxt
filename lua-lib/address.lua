-- Internal structural validation; does not enforce a checksum.
local M = {}

---@param value string Address to check.
---@param name? string Field name to include in an error.
---@return string address The checked address, unchanged.
function M.validate(value, name)
    assert(type(value) == "string" and #value == 42
        and value:match("^0x[0-9a-fA-F]+$"),
        (name or "address") .. " must be 0x followed by 40 hex digits")
    return value
end

return M
