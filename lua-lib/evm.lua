local Tx = require("lua-lib.tx").Tx
local Op = require("lua-lib.ops").Op
local M = {}

---@param op Op
---@return any ... Values returned when the engine resumes this coroutine.
local function send_op(op)
    return coroutine.yield(op)
end

---@param tx Tx
---@return string hash Broadcast transaction hash, not a receipt.
function M.send(tx)
    assert(type(tx) == "table" and getmetatable(tx) == Tx, "expected Tx")
    return send_op(Op.new("call", tx.call, tx.to))
end

---@return integer chain_id
function M.chain_id()
    return send_op(Op.new("get_chain_id"))
end

---@param private_key string 0x-prefixed, 32-byte private key.
---@return boolean success
function M.set_wallet(private_key)
    return send_op(Op.new("set_wallet", private_key))
end

---@return integer nonce The engine returns -1 if no wallet is set.
function M.nonce()
    return send_op(Op.new("get_nonce"))
end

---@param gas Gas
---@return boolean success
function M.set_gas(gas)
    return send_op(Op.new("set_gas", gas))
end

---@return string balance Native balance in wei, as a 0x hex quantity.
function M.balance()
    return send_op(Op.new("get_balance"))
end

---@return Wallet
function M.create_wallet()
    return send_op(Op.new("wallet_create"))
end

return M
