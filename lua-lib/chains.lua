--[[
    Names and IDs for known mainnet chains. 
]]--

---@class Chain
---@field id integer
---@field name string

---@type table<string, Chain>
local M = {}

M.ETHEREUM  = { id = 1,     name = "Ethereum" }
M.BASE      = { id = 8453,  name = "Base" }
M.ARBITRUM  = { id = 42161, name = "Arbitrum One" }
M.OPTIMISM  = { id = 10,    name = "OP Mainnet" }
M.POLYGON   = { id = 137,   name = "Polygon PoS" }
M.BNB       = { id = 56,    name = "BNB Smart Chain" }
M.AVALANCHE = { id = 43114, name = "Avalanche C-Chain" }
M.ROBINHOOD = { id = 4663,  name = "Robinhood Chain" }

return M
