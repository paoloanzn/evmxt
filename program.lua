--[[
    Main applicaiton Lua entry point.

    Program lifecyle and control
    flow must be defined here.

    This file must return a table containing
    a "main" key with a function() as value.
    The content of function() is what gets
    exectuted by the engine.
]]--

-- local Call = require("lua-lib.call").Call
local Op = require("lua-lib.ops").Op
local wallet_store = require("lua-lib.wallet")

---@param op Op
---@return any
local function send_op(op)
    return coroutine.yield(op)
end

return {
    main = function()
        local chain_id = send_op(
            Op.new("get_chain_id", nil)
        )

        print(string.format("From lua, chain_id = %s", chain_id))

        local address, err = wallet_store.search({ name = "sec" })
        assert(not err, err)

        if not address then
            local wallet = send_op(
                Op.new("wallet_create", nil)
            )
            print("New wallet:")
            for _, key in ipairs({"address", "private_key", "public_key"}) do
                print(string.format("%s: %s", tostring(key), tostring(wallet[key])))
            end

            assert(wallet_store.store(
                        wallet["address"],
                        wallet["private_key"],
                        { name = "sec" }))
            assert(send_op(Op.new("set_wallet", wallet["private_key"])))
        else
            local wallet = assert(wallet_store.load(address))
            assert(send_op(Op.new("set_wallet", tostring(wallet["private_key"]))))
            print("Loaded wallet:")
            for _, key in ipairs({"address", "private_key", "public_key"}) do
                print(string.format("%s: %s", tostring(key), tostring(wallet[key])))
            end
        end

        local nonce = send_op(Op.new("get_nonce", nil))
        if nonce < 0 then 
            print("Failed to get nonce, verify an active wallet is set")
        else
            print(string.format("Nonce: %d", nonce))
        end
    end
}
