--[[
    Store Ethereum wallets in the OS keychain (macOS Security or Linux
    Secret Service). Build c-bindings/bindings.so before loading this module;
    see c-bindings/README.md for the standalone build instructions.

    Metadata is an optional table of string keys and values. It can
    be used to search for wallets as alternative to addresses.
    The main retrivial remains the one by address. Two metadata belonging
    to two different wallets can exists, in that case search returns
    the smallest matching address. 

    Usage:
    local wallet = require("lua-lib.wallet")
    assert(wallet.store(address, private_key, { name = "main" }))
    local saved = assert(wallet.load(address))
    local found = wallet.search({ name = "main" })
    assert(wallet.delete(found))
]]--

---@class Wallet
---@field private_key string 0x-prefixed, 32-byte private key.
---@field public_key string 0x-prefixed, 64-byte public key (without the 04 prefix).
---@field address string 0x-prefixed, 20-byte address.

-- Load the native binding beside this module.
local directory = assert(debug.getinfo(1, "S").source:match("^@(.*/)"),
    "Cannot locate the wallet module directory")
local open_native = assert(package.loadlib(directory .. "c-bindings/bindings.so",
    "luaopen_wallet_native"))
local native = open_native()
local M = {}

---@param value string
---@param bytes integer
---@return string
local function hex(value, bytes)
    assert(type(value) == "string" and #value == 2 + bytes * 2
        and value:match("^0x[0-9a-fA-F]+$"), "Invalid wallet hex value")

    return value:lower()
end

-- Encode lengths and bytes so metadata can contain any characters.
---@param value string
---@return string
local function encode(value)
    return #value .. ":" .. (value:gsub(".", function(c)
        return string.format("%02x", c:byte())
    end))
end

-- Keep omitted metadata distinct from an empty table.
---@param value? table<string, string>
---@return string
local function metadata(value)
    if value == nil then
        return "n"
    end

    assert(type(value) == "table", "Metadata must be a table of string keys and values")

    local keys = {}
    local parts = { "t" }

    for key, item in pairs(value) do
        assert(type(key) == "string" and type(item) == "string",
            "Metadata keys and values must be strings")
        keys[#keys + 1] = key
    end

    -- Table order must not affect matching.
    table.sort(keys)

    for _, key in ipairs(keys) do
        parts[#parts + 1] = encode(key) .. encode(value[key])
    end

    return table.concat(parts)
end

-- Return keychain errors to the caller instead of raising them.
---@overload fun(action: 'load', address: string): string|nil, string|nil
---@overload fun(action: 'list'): string[]|nil, string|nil
---@param action 'store'|'delete'
---@param address? string
---@param payload? string
---@return boolean|nil
---@return string|nil error
local function request(action, address, payload)
    local ok, result = pcall(native.request, action, address, payload)

    if ok then
        return result
    end

    return nil, result
end

---Store or replace a wallet.
---@param address string
---@param private_key string
---@param tags? table<string, string>
---@return boolean
---@return string|nil error
function M.store(address, private_key, tags)
    address = hex(address, 20)
    private_key = hex(private_key, 32)

    local _, derived = native.derive(private_key)
    assert(address == derived, "Private key does not match address")

    -- Store the key and metadata together in the keychain.
    local ok, err = request("store", address, private_key .. metadata(tags))

    return ok == true, err
end

---@param address string
---@return Wallet|nil
---@return string|nil error
function M.load(address)
    address = hex(address, 20)
    local payload, err = request("load", address)

    if not payload then
        return nil, err
    end

    -- The first 66 characters hold the private key, including 0x.
    local private_key = hex(payload:sub(1, 66), 32)
    local public_key, derived = native.derive(private_key)
    assert(address == derived, "Stored private key does not match address")

    return {
        private_key = private_key,
        public_key = public_key,
        address = address
    }
end

---@param tags table<string, string>
---@return string|nil address
---@return string|nil error
function M.search(tags)
    local wanted = metadata(tags)
    local addresses, err = request("list")

    if not addresses then
        return nil, err
    end

    table.sort(addresses)

    for _, address in ipairs(addresses) do
        local payload, load_error = request("load", address)

        if load_error then
            return nil, load_error
        end

        if payload and payload:sub(67) == wanted then
            return address
        end
    end
end

---@param address string
---@return boolean
---@return string|nil error
function M.delete(address)
    local ok, err = request("delete", hex(address, 20))

    return ok == true, err
end

return M
