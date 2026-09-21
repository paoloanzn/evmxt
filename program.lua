--[[
    Main applicaiton Lua entry point.

    Program lifecyle and control
    flow must be defined here.

    This file must return a table containing
    a "main" key with a function() as value.
    The content of function() is what gets
    exectuted by the engine.
]]--

local Call = require("lua-lib.call").Call
local Op = require("lua-lib.ops").Op
local Gas = require("lua-lib.gas").Gas
local wallet_store = require("lua-lib.wallet")

-- Native USDC on Base: https://developers.circle.com/stablecoins/usdc-contract-addresses
local usdc = "0x833589fCD6eDb6E08f4c7C32D4f71b54bdA02913"
local recipient = "0xE8134C7574469f4DdD807e456C4eF8De741F8eee"
local amount = 500000 -- 0.5 USDC, with 6 decimals.
local transfer = Call.compile("transfer", { "address", "uint256" })

---@param op Op
local function send_op(op)
    return coroutine.yield(op)
end

return {
    main = function()
        local chain_id = send_op(
            Op.new("get_chain_id", nil)
        )

        print(string.format("From lua, chain_id = %s", chain_id))
        assert(chain_id == 8453, "This USDC transfer requires Base mainnet (8453)")

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

        local eth_balance = send_op(Op.new("get_balance", nil))
        if not eth_balance then
            print("Failed to get eth balance")
        else
            print(string.format("Eth balance: %s", eth_balance))
        end

        assert(nonce >= 0, "USDC transfer requires an active wallet nonce")
        -- Explicit settings, not a live fee estimate. Fees are paid in ETH.
        assert(send_op(Op.new("set_gas", Gas.new({
            gas_limit = 150000,
            max_priority_fee_per_gas = 1000000, -- 0.001 gwei
            max_fee_per_gas = 1000000000,       -- 1 gwei
        }))))

        -- Prepare both calls before submitting. The runtime advances the nonce
        -- after each accepted submission; do not fetch it again between calls.
        assert(nonce < math.maxinteger, "Two transfers require room for consecutive nonces")
        local first = Op.new("call", transfer(recipient, amount), usdc)
        local second = Op.new("call", transfer(recipient, amount), usdc)

        -- Submit back-to-back, without waiting for either transaction to be mined.
        local first_hash = assert(send_op(first), "First USDC transfer submission failed")
        local second_hash = assert(send_op(second), "Second USDC transfer submission failed")

        print(string.format("Transfer 1: %.6f USDC, nonce %d, hash %s",
            amount / 1000000, nonce, first_hash))
        print(string.format("Transfer 2: %.6f USDC, nonce %d, hash %s",
            amount / 1000000, nonce + 1, second_hash))
        print("Compare the block numbers of both receipts to check same-block inclusion.")
    end
}
