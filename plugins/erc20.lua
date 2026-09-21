local Contract = require("lua-lib.contract").Contract
local address = require("lua-lib.address")

---@class ERC20
---@field address string Token contract address.
---@field private _contract Contract
local ERC20 = {}
ERC20.__index = ERC20

---@param token_address string 0x followed by 40 hex digits.
---@return ERC20
function ERC20.new(token_address)
    local contract = Contract.new({
        address = token_address,
        methods = {
            approve = { "address", "uint256" },
            transfer = { "address", "uint256" },
        },
    })
    return setmetatable({ address = token_address, _contract = contract }, ERC20)
end

---@param spender string Address allowed to spend the tokens.
---@param amount integer|string Amount in token base units: nonnegative integer or 0x hex string.
---@return Tx
function ERC20:approve(spender, amount)
    address.validate(spender, "spender")
    return self._contract:tx("approve", spender, amount)
end

---@param to string Address receiving the tokens.
---@param amount integer|string Amount in token base units: nonnegative integer or 0x hex string.
---@return Tx
function ERC20:transfer(to, amount)
    address.validate(to, "to")
    return self._contract:tx("transfer", to, amount)
end

return ERC20
