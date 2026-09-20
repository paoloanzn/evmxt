--[[
    This module handles the work behind Call.compile by turning a function
    name and its ABI parameter types into a reusable description, containing
    the canonical signature, its four-byte selector and a tree of parameter
    descriptors that the C engine can read without parsing type strings.

    The parser follows nested tuples and arrays to record each parameter's
    kind, size and whether its encoding is dynamic, while expanding aliases
    such as uint to uint256 before building the signature and passing it to
    the existing C selector function.

    Compilation depends only on the function definition, so argument values
    and the destination are supplied later through Call and Op, allowing the
    same descriptors to be reused across transactions.

    Supported types are address, bool, integers, bytes and string, together
    with tuples and arrays built from them using the following grammar:

    type := (elementary | '(' [type (',' type)*] ')') ('[' digits? ']')*

    The type and signature rules follow the Solidity ABI specification:
    https://docs.solidity.org/en/latest/abi-spec.html
]]--
local M = {}

-- Type tags for the Lua/C boundary; distinct from abi.h's encoded value kinds.
local ABI = {
    ADDRESS = 1, BOOL = 2, UINT = 3, INT = 4,
    FIXED_BYTES = 5, BYTES = 6, STRING = 7,
    ARRAY = 8, FIXED_ARRAY = 9, TUPLE = 10,
}
M.ABI = ABI

local elementary = {
    address = ABI.ADDRESS, bool = ABI.BOOL,
    bytes = ABI.BYTES, string = ABI.STRING,
}

local function scalar(token)
    local kind = elementary[token]
    if kind then
        return { kind = kind, dynamic = token == "bytes" or token == "string" }, token
    end

    local base, width = token:match("^([a-z]+)([0-9]*)$")
    if base == "uint" or base == "int" then
        local bits = width == "" and 256 or tonumber(width)
        assert(bits and bits >= 8 and bits <= 256 and bits % 8 == 0
            and (width == "" or width == tostring(bits)), "Invalid integer width: " .. token)
        return { kind = base == "uint" and ABI.UINT or ABI.INT,
            bits = bits, dynamic = false }, base .. bits
    end
    if base == "bytes" then
        local size = tonumber(width)
        assert(size and size >= 1 and size <= 32 and width == tostring(size),
            "Invalid byte width: " .. token)
        return { kind = ABI.FIXED_BYTES, size = size, dynamic = false }, token
    end
    error("Unsupported ABI type: " .. token)
end

-- Recursive descent emits descriptors and canonical names together.
local function parse(source)
    assert(type(source) == "string", "ABI type must be a string")
    local pos = 1

    local function match_at(pattern)
        local first, last = source:find(pattern, pos)
        if first ~= pos then return nil end
        return source:sub(first, last)
    end

    local function consume(character)
        if source:sub(pos, pos) ~= character then return false end
        pos = pos + 1
        return true
    end

    local function parse_type(depth)
        assert(depth <= 64, "ABI type exceeds maximum depth (64)")
        local node, canonical
        local height = 1
        if consume("(") then
            local components, names = {}, {}
            local dynamic = false
            if not consume(")") then
                repeat
                    local child, name, child_height = parse_type(depth + 1)
                    components[#components + 1] = child
                    names[#names + 1] = name
                    dynamic = dynamic or child.dynamic
                    height = math.max(height, child_height + 1)
                until not consume(",")
                assert(consume(")"), "Expected ')' at byte " .. pos .. " in " .. source)
            end
            node = { kind = ABI.TUPLE, components = components, dynamic = dynamic }
            canonical = "(" .. table.concat(names, ",") .. ")"
        else
            local token = match_at("[a-z]+[0-9]*")
            assert(token, "Expected ABI type at byte " .. pos .. " in " .. source)
            pos = pos + #token
            node, canonical = scalar(token)
        end

        while consume("[") do
            local digits = match_at("[0-9]*")
            pos = pos + #digits
            assert(consume("]"), "Expected ']' at byte " .. pos .. " in " .. source)
            local length
            if digits ~= "" then
                length = math.tointeger(tonumber(digits))
                assert(length and length >= 0 and tostring(length) == digits,
                    "Invalid array length: " .. digits)
            end
            node = {
                kind = length and ABI.FIXED_ARRAY or ABI.ARRAY,
                element = node,
                length = length,
                dynamic = length == nil or node.dynamic,
            }
            canonical = canonical .. "[" .. digits .. "]"
            height = height + 1
            assert(depth + height - 1 <= 64, "ABI type exceeds maximum depth (64)")
        end
        return node, canonical, height
    end

    local node, canonical = parse_type(1)
    assert(pos == #source + 1, "Unexpected input at byte " .. pos .. " in " .. source)
    return node, canonical
end

-- Load only when compiling, so importing Call/Op needs no native module.
local native
local directory = assert(debug.getinfo(1, "S").source:match("^@(.*/)"))
local function selector(signature)
    if not native then
        native = assert(package.loadlib(directory .. "c-bindings/bindings.so",
            "luaopen_compiler_native"))()
    end
    return native.selector(signature)
end

-- Compile types into a value-independent descriptor tree and function selector.
-- Arrays have element/length fields; tuples have components; bytesN has size.
-- Array lengths must fit a Lua integer. Nesting is bounded as in abi.h.
function M.compile(name, types)
    assert(type(name) == "string" and name:match("^[a-zA-Z_][a-zA-Z0-9_]*$"),
        "Invalid function name")
    assert(type(types) == "table", "Parameters must be an array of ABI types")
    local count = 0
    for key in pairs(types) do
        assert(type(key) == "number" and key % 1 == 0 and key >= 1,
            "Invalid parameter index")
        count = count + 1
    end

    local params, names = {}, {}
    for i = 1, count do
        assert(rawget(types, i) ~= nil, "Sparse parameters")
        params[i], names[i] = parse(types[i])
    end
    local signature = name .. "(" .. table.concat(names, ",") .. ")"
    return { selector = selector(signature), signature = signature, params = params }
end

return M
