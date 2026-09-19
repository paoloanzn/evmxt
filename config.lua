require "dotenv".config()

local function print_error_and_exit(message)
	print(string.format("error: %s", message))
	os.exit(1)
end

local function init()
	local rpc_url = os.getenv("RPC_URL")
	if rpc_url == nil then print_error_and_exit("RPC_URL is missing") end
	local config = {
		rpc_url = rpc_url
	}
	return config
end

global_config = init()
