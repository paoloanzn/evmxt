--[[
    The Cli object defines an interactive and generic commands
    terminal interface. A Command object gives one of those commands
    a name, such as /greet, and says which function to run
    and what arguments to give it. Add a Command object to the Cli
    so it knows about it. The Cli also has live incremental auto-suggestions.
    It keeps reading input until the user exits or a command returns false.

    Usage: create a command, register it, then start the CLI:

    local cli = require("cli")
    local Cli, Command = cli.Cli, cli.Command
    local greet = Command.compile("/greet", function(name)
        print("Hello, " .. name)
    end, "world")
    Cli.add(greet)
    Cli.start()
]]--

local COMMAND_PREFIX = "/"

---@class Command
---@field name string
---@field func function
---@field args table
---@field arg_count integer
local Command = {}
Command.__index = Command

---@param name string
---@param func function
---@return Command
function Command.compile(name, func, ...)
    assert(type(name) == "string" and name:match("^/[^%s/]+$"), "Invalid command name")
    assert(type(func) == "function", "Command must be a function")
    return setmetatable({
        name = name,
        func = func,
        args = { ... },
        arg_count = select("#", ...),
    }, Command)
end

function Command:execute()
    return self.func(table.unpack(self.args, 1, self.arg_count))
end

local function not_implemented()
    print("Not implemented")
end

local function quit()
    return false
end

local M = {}

---@type table<string, Command>
local commands = {}
local Cli = {}

---@param command Command
function Cli.add(command)
    assert(getmetatable(command) == Command, "Expected a Command")
    assert(commands[command.name] == nil, "Command already exists: " .. command.name)
    commands[command.name] = command
end

Cli.add(Command.compile("/help", not_implemented))
Cli.add(Command.compile("/end", quit))
Cli.add(Command.compile("/exit", quit))
Cli.add(Command.compile("/q", quit))

local function execute_command(command)
    local entry = commands[command]
    if entry == nil then
        print("Unknown command: " .. command)
        return
    end
    return entry:execute()
end

local function matches_for(prefix)
    if prefix:sub(1, 1) ~= COMMAND_PREFIX then return {} end
    local matches = {}
    for name in pairs(commands) do
        if name:sub(1, #prefix) == prefix then
            matches[#matches + 1] = name
        end
    end
    table.sort(matches, function(a, b)
        if #a ~= #b then return #a < #b end
        return a < b
    end)
    return matches
end

local function complete(prefix)
    local matches = matches_for(prefix)
    if #matches == 0 then return prefix end
    local common = matches[1]
    for i = 2, #matches do
        while matches[i]:sub(1, #common) ~= common do
            common = common:sub(1, -2)
        end
    end
    return common
end

local function delete_word(input)
    return (input:gsub("%s*[^%s]+%s*$", ""))
end

local function redraw(input)
    local matches = matches_for(input)
    io.write("\r\27[J> ", input)
    if #matches > 0 then
        io.write("\n\27[2m", table.concat(matches, "\n"), "\27[0m")
        io.write(string.format("\27[%dA\r\27[%dC", #matches, #input + 2))
    end
    io.flush()
end

local function read_terminal_line()
    local input = ""
    redraw(input)
    while true do
        local char = io.read(1)
        if not char then return nil end
        local byte = char:byte()
        if char == "\n" or char == "\r" then
            io.write("\r\27[J> ", input, "\n")
            return input
        elseif char == "\t" then
            input = complete(input)
        elseif byte == 27 then
            -- macOS Option+Delete sends Escape followed by Backspace.
            local next_char = io.read(1)
            if next_char == "\127" or next_char == "\b" then
                input = delete_word(input)
            elseif next_char == "[" then
                -- Consume an arrow/function key sequence without adding it to input.
                repeat
                    next_char = io.read(1)
                until not next_char or next_char:match("[%a~]")
            end
        elseif byte == 127 or byte == 8 then
            input = input:sub(1, -2)
        elseif byte == 23 then
            input = delete_word(input)
        elseif byte == 4 then
            if input == "" then
                io.write("\r\27[J\n")
                return nil
            end
        elseif byte >= 32 and byte ~= 127 then
            input = input .. char
        end
        redraw(input)
    end
end

local function read_input(reader)
    local input = reader()
    if input == nil then return false end
    if string.sub(input, 1, 1) == COMMAND_PREFIX then
        if execute_command(input) == false then return false end
    end
    return true
end

function Cli.start()
    local handle = io.popen("stty -g 2>/dev/null < /dev/tty", "r")
    local settings = handle and handle:read("*l")
    if handle then handle:close() end
    if not settings or not settings:match("^[%w:]+$") then
        while read_input(io.read) do end
        return
    end
    os.execute("stty -icanon -echo min 1 time 0 < /dev/tty")
    local ok, err = xpcall(function()
        while read_input(read_terminal_line) do end
    end, debug.traceback)
    os.execute("stty " .. settings .. " < /dev/tty")
    if not ok then error(err, 0) end
end

M.Cli = Cli
M.Command = Command

return M
