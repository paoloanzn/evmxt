--[[
    Original author: https://github.com/kayibea/dotenv
--]]
local M = {}

---@type table<string, string>
local env = {}

local original_getenv = _G.os.getenv

---@param key string
---@return string?
local function getenv(key)
  if env[key] then
    return env[key]
  end

  return original_getenv(key)
end

function M.config()
  local f, err = io.open(".env", "r")
  if not f then
    error("Could not open .env file: " .. tostring(err))
  end

  for line in f:lines() do
    if not line:match("^%s*[#;]") and not line:match("^%s*$") then
      local key, value = line:match("^%s*([^=]+)%s*=%s*(.-)%s*$")

      if key and value then
        key = key:gsub("^%s+", ""):gsub("%s+$", "")
        value = value:gsub('^"(.*)"$', '%1'):gsub("^'(.*)'$", '%1')
        value = value:gsub("\\n", "\n"):gsub("\\r", "\r"):gsub("\\t", "\t")
        value = value:gsub("\\\\", "\\")
        value = value:gsub("\\\"", "\""):gsub("\\\'", "\'")
        env[key] = value
      end
    end
  end

  f:close()
  _G.os.getenv = getenv
end

return M