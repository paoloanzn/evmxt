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

        local wallet = send_op(
            Op.new("wallet_create", nil)
        )
        print("New wallet:")
        for _idx, key in ipairs({"address", "private_key", "public_key"}) do
            print(string.format("%s: %s", tostring(key), tostring(wallet[key])))
        end
    end
}
