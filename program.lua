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
    end
}