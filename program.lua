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
local tokens = require("lua-lib.tokens")
local cli = require("cli")
local Cli, Command = cli.Cli, cli.Command

local Chain = require("lua-lib.chains").BASE
local ERC20 = require("plugins.erc_20.erc20")

--[[
        The program searches for a wallet with
        the corresponsing metadata from the OS
        keychain. If not found it creates and save
        a new one for the following runs.

        Note: The wallet is stored and recovered
        from keychain, this feature is not available
        without it. However the wallet, including
        its private_key is loaded un-encrypted
        in memory for the program to use it; treat
        it as an emphimeral hot wallet. Do not store
        permanents funds on it.
]]--
local wallet_metadata = { name = "sec" }

---@param op Op
local function send_op(op)
    return coroutine.yield(op)
end

---@param recepient string
---@param token_symbol string
---@param amount string Amount in whole token units, optionally with a decimal fraction.
local function transfer_(recepient, token_symbol, amount)
    local deployments = tokens[token_symbol]
    local token = deployments and deployments[Chain]
    if type(token) ~= "table" then
        print("Unknown token on " .. Chain.name .. ": " .. tostring(token_symbol))
        return
    end
    local whole, fraction = tostring(amount):match("^(%d+)%.(%d+)$")
    if not whole then
        whole = tostring(amount):match("^(%d+)$")
        fraction = ""
    end
    if not whole or #fraction > token.decimals then
        print("Invalid amount: use a non-negative decimal with at most " .. token.decimals .. " decimal places")
        return
    end
    -- Append decimal places as digits so conversion never rounds a token amount.
    local units = tonumber(whole .. fraction .. string.rep("0", token.decimals - #fraction))
    if math.type(units) ~= "integer" then
        print("Amount exceeds the supported Lua integer range")
        return
    end
    local op = Op.new(
                        "call",
                        ERC20.transfer(recepient, units),
                        token.address
    )
    local tx_hash = send_op(op)
    assert(tx_hash, "Transaction submission failed")
    print(string.format("Success, tx: %s", tx_hash))
end

-- Commands
Cli.add(Command.compile("/transfer", transfer_))

-- init
local function init()
        local chain_id = send_op(
            Op.new("get_chain_id", nil)
        )
        assert(chain_id == Chain.id, string.format(
            "chain_id is set to %s (%s), but rpc is pointing to %s",
            Chain.id, Chain.name, chain_id))

        -- Wallet init
        local wallet = nil
        local address, err = wallet_store.search({ name = "sec" })
        assert(not err, err)

        if not address then
            wallet = send_op(
                Op.new("wallet_create", nil)
            )

            assert(wallet_store.store(
                        wallet["address"],
                        wallet["private_key"],
                        { name = "sec" }))
            print("New wallet created!")
            assert(send_op(Op.new("set_wallet", wallet["private_key"])))
        else
            wallet = assert(wallet_store.load(address))
            assert(send_op(Op.new("set_wallet", tostring(wallet["private_key"]))))
            print("Loaded wallet:")
            for _, key in ipairs({"address", "public_key"}) do
                print(string.format("%s: %s", tostring(key), tostring(wallet[key])))
            end
        end
        print("Wallet:")
        for _, key in ipairs({"address", "public_key"}) do
            if key == "address" then
                print(string.format("%s: %s", 
                                    tostring(key),
                                    tostring(wallet[key])
                                    ))
            else
            print(string.format("%s: %s", tostring(key), 
                string.format(
                    "%s...%s", 
                    string.sub(wallet[key], 1, 5),
                    string.sub(wallet[key], 131-5, 131))
                ))
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

        -- Gas settings
        assert(send_op(Op.new("set_gas", Gas.new({
            gas_limit = 150000,
            max_priority_fee_per_gas = 1000000, -- 0.001 gwei
            max_fee_per_gas = 1000000000,       -- 1 gwei
        }))))
end

return {
    main = function()
        init()
        Cli.start()
    end
}
