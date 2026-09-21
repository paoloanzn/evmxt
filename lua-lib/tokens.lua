--[[
    Known token addresses, grouped by asset and numeric chain ID.
    For example, Tokens.USDC[8453] describes native USDC on Base.
]]--



local M = {}
local Chain = require("lua-lib.chains")

---@class TokenDescriptor
---@field address string 
---@field symbol string 
---@field decimals integer 

---@type table<Chain, TokenDescriptor>
M.USDC = {
    [Chain.ETHEREUM] = {
        address = "0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48",
        symbol = "USDC",
        decimals = 6,
    },
    [Chain.BASE] = {
        address = "0x833589fCD6eDb6E08f4c7C32D4f71b54bdA02913",
        symbol = "USDC",
        decimals = 6,
    },
    [Chain.ARBITRUM] = {
        address = "0xaf88d065e77c8cC2239327C5EDb3A432268e5831",
        symbol = "USDC",
        decimals = 6,
    },
    [Chain.OPTIMISM] = {
        address = "0x0b2C639c533813f4Aa9D7837CAf62653d097Ff85",
        symbol = "USDC",
        decimals = 6,
    },
    [Chain.POLYGON] = {
        address = "0x3c499c542cEF5E3811e1192ce70d8cC03d5c3359",
        symbol = "USDC",
        decimals = 6,
    },
    [Chain.AVALANCHE] = {
        address = "0xB97EF9Ef8734C71904D8002F8b6Bc66Dd9c48a6E",
        symbol = "USDC",
        decimals = 6,
    },
}

---@type table<Chain, TokenDescriptor>
M.WETH = {
    [Chain.ETHEREUM] = {
        address = "0xC02aaA39b223FE8D0A0e5C4F27eAD9083C756Cc2",
        symbol = "WETH",
        decimals = 18,
    },
    [Chain.BASE] = {
        address = "0x4200000000000000000000000000000000000006",
        symbol = "WETH",
        decimals = 18,
    },
    [Chain.ARBITRUM] = {
        address = "0x82aF49447D8a07e3bd95BD0d56f35241523fBab1",
        symbol = "WETH",
        decimals = 18,
    },
    [Chain.OPTIMISM] = {
        address = "0x4200000000000000000000000000000000000006",
        symbol = "WETH",
        decimals = 18,
    },
}

return M
