--[[
    Known Uniswap V3 contracts, looked up by Chain descriptor.
    These are saved addresses, so loading this file never contacts a network.
    Treat the tables as read-only. Missing chains have no default.

    Verified against the official per-chain sources below on 2026-09-21.
    router means SwapRouter02 (the seven-field, no-deadline swap ABI).
    quoter means QuoterV2; positionManager means NonfungiblePositionManager.
    ABI: https://github.com/Uniswap/swap-router-contracts/blob/main/contracts/interfaces/IV3SwapRouter.sol
]]--

---@class UniswapV3Deployment
---@field factory? string UniswapV3Factory address, if known.
---@field router string SwapRouter02 address.
---@field quoter? string QuoterV2 address, if known.
---@field positionManager? string NonfungiblePositionManager address, if known.

local Chain = require("lua-lib.chains")

---@type table<Chain, UniswapV3Deployment>
local M = {}

-- https://developers.uniswap.org/docs/protocols/v3/deployments/v3-ethereum-deployments
M[Chain.ETHEREUM] = {
    factory = "0x1F98431c8aD98523631AE4a59f267346ea31F984",
    router = "0x68b3465833fb72A70ecDF485E0e4C7bD8665Fc45",
    quoter = "0x61fFE014bA17989E743c5F6cB21bF9697530B21e",
    positionManager = "0xC36442b4a4522E871399CD717aBDD847Ab11FE88",
}

-- https://developers.uniswap.org/docs/protocols/v3/deployments/v3-base-deployments
M[Chain.BASE] = {
    factory = "0x33128a8fC17869897dcE68Ed026d694621f6FDfD",
    router = "0x2626664c2603336E57B271c5C0b26F421741e481",
    quoter = "0x3d4e44Eb1374240CE5F1B871ab261CD16335B76a",
    positionManager = "0x03a520b32C04BF3bEEf7BEb72E919cf822Ed34f1",
}

-- https://developers.uniswap.org/docs/protocols/v3/deployments/v3-arbitrum-deployments
M[Chain.ARBITRUM] = {
    factory = "0x1F98431c8aD98523631AE4a59f267346ea31F984",
    router = "0x68b3465833fb72A70ecDF485E0e4C7bD8665Fc45",
    quoter = "0x61fFE014bA17989E743c5F6cB21bF9697530B21e",
    positionManager = "0xC36442b4a4522E871399CD717aBDD847Ab11FE88",
}

-- https://developers.uniswap.org/docs/protocols/v3/deployments/v3-optimism-deployments
M[Chain.OPTIMISM] = {
    factory = "0x1F98431c8aD98523631AE4a59f267346ea31F984",
    router = "0x68b3465833fb72A70ecDF485E0e4C7bD8665Fc45",
    quoter = "0x61fFE014bA17989E743c5F6cB21bF9697530B21e",
    positionManager = "0xC36442b4a4522E871399CD717aBDD847Ab11FE88",
}

-- https://developers.uniswap.org/docs/protocols/v3/deployments/v3-polygon-deployments
M[Chain.POLYGON] = {
    factory = "0x1F98431c8aD98523631AE4a59f267346ea31F984",
    router = "0x68b3465833fb72A70ecDF485E0e4C7bD8665Fc45",
    quoter = "0x61fFE014bA17989E743c5F6cB21bF9697530B21e",
    positionManager = "0xC36442b4a4522E871399CD717aBDD847Ab11FE88",
}

-- https://developers.uniswap.org/docs/protocols/v3/deployments/v3-bnb-deployments
M[Chain.BNB] = {
    factory = "0xdB1d10011AD0Ff90774D0C6Bb92e5C5c8b4461F7",
    router = "0xB971eF87ede563556b2ED4b1C0b0019111Dd85d2",
    quoter = "0x78D78E420Da98ad378D7799bE8f4AF69033EB077",
    positionManager = "0x7b8A01B39D58278b5DE7e48c8449c9f4F5170613",
}

-- https://developers.uniswap.org/docs/protocols/v3/deployments/v3-avalanche-deployments
M[Chain.AVALANCHE] = {
    factory = "0x740b1c1de25031C31FF4fC9A62f554A55cdC1baD",
    router = "0xbb00FF08d01D300023C629E8fFfFcb65A5a578cE",
    quoter = "0xbe0F5544EC67e9B3b2D979aaA43f18Fd87E6257F",
    positionManager = "0x655C406EBFa14EE2006250925e54ec43AD184f8B",
}

-- https://developers.uniswap.org/docs/protocols/v3/deployments/v3-robinhood-chain-deployments
M[Chain.ROBINHOOD] = {
    factory = "0x1f7d7550b1b028f7571e69a784071f0205fd2efa",
    router = "0xcaf681a66d020601342297493863e78c959e5cb2",
    quoter = "0x33e885ed0ec9bf04ecfb19341582aadcb4c8a9e7",
    positionManager = "0x73991a25c818bf1f1128deaab1492d45638de0d3",
}

return M
