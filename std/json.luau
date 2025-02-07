--!strict

local json = {
	--- Not actually a nil value, a newproxy stand-in for a null value since Luau has no actual representation of `null`
	NULL = newproxy() :: nil,
}

type JSONPrimitive = nil | number | string | boolean
type Object = { [string]: Value }
type Array = { Value }
export type Value = JSONPrimitive | Array | Object

-- serialization

type SerializerState = {
	buf: buffer,
	cursor: number,
	prettyPrint: boolean,
	depth: number,
}

local function check_state(state: SerializerState, len: number)
	local cur_len = buffer.len(state.buf)

	if state.cursor + len < cur_len then
		return
	end

	local new_buffer = buffer.create(cur_len * 2)

	buffer.copy(new_buffer, 0, state.buf)

	state.buf = new_buffer
end

local function write_byte(state: SerializerState, byte: number)
	check_state(state, 1)

	buffer.writeu8(state.buf, state.cursor, byte)

	state.cursor += 1
end

local function write_spaces(state: SerializerState)
	if state.depth == 0 then
		return
	end

	if state.prettyPrint then
		check_state(state, state.depth * 4)

		for i = 0, state.depth do
			buffer.writeu32(state.buf, state.cursor, 0x_20_20_20_20)
			state.cursor += 4
		end
	else
		buffer.writeu8(state.buf, state.cursor, string.byte(" "))

		state.cursor += 1
	end
end

local function write_string(state: SerializerState, str: string)
	check_state(state, #str)

	buffer.writestring(state.buf, state.cursor, str)

	state.cursor += #str
end

local serialize_any

local ESCAPE_MAP = {
	[0x5C] = string.byte("\\"), -- 5C = '\'
	[0x08] = string.byte("b"),
	[0x0C] = string.byte("f"),
	[0x0A] = string.byte("n"),
	[0x0D] = string.byte("r"),
	[0x09] = string.byte("t"),
}

local scratch_unicode_buffer = buffer.create(4)

local function serialize_string(state: SerializerState, str: string)
	check_state(state, #str)

	write_byte(state, string.byte('"'))

	for pos, codepoint in utf8.codes(str) do
		if ESCAPE_MAP[codepoint] then
			write_byte(state, string.byte("\\"))
			write_byte(state, ESCAPE_MAP[codepoint])
		elseif codepoint < 0x0020 or codepoint > 0x10FFFF then
			buffer.writeu32(scratch_unicode_buffer, 0, codepoint)
			write_string(state, "\\u{")

			write_string(state, string.format("%04x", codepoint))

			write_byte(state, string.byte("}"))
		else
			write_string(state, utf8.char(codepoint))
		end
	end

	write_byte(state, string.byte('"'))
end

local function serialize_array(state: SerializerState, array: Array)
	state.depth += 1

	write_byte(state, string.byte("["))

	if state.prettyPrint and #array ~= 0 then
		write_byte(state, string.byte("\n"))
	end

	for i, value in array do
		if i ~= 1 then
			write_byte(state, string.byte(","))

			if state.prettyPrint then
				write_byte(state, string.byte("\n"))
			end
		end

		if i ~= 1 or state.prettyPrint then
			write_spaces(state)
		end

		serialize_any(state, value)
	end

	state.depth -= 1

	if state.prettyPrint and #array ~= 0 then
		write_byte(state, string.byte("\n"))
		write_spaces(state)
	end

	write_byte(state, string.byte("]"))
end

local function serialize_table(state: SerializerState, object: Object)
	write_byte(state, string.byte("{"))

	if state.prettyPrint then
		write_byte(state, string.byte("\n"))
	end

	state.depth += 1

	local first = true
	for key, value in object do
		if not first then
			write_byte(state, string.byte(","))
			write_byte(state, string.byte(" "))

			if state.prettyPrint then
				write_byte(state, string.byte("\n"))
			end
		end

		first = false

		write_spaces(state)

		write_byte(state, string.byte('"'))
		write_string(state, key)
		write_string(state, '": ')

		serialize_any(state, value)
	end

	if state.prettyPrint then
		write_byte(state, string.byte("\n"))
	end

	state.depth -= 1

	write_spaces(state)

	write_byte(state, string.byte("}"))
end

serialize_any = function(state: SerializerState, value: Value)
	local value_type = type(value)

	if value == json.NULL then
		write_string(state, "null")
	elseif value_type == "boolean" then
		write_string(state, if value then "true" else "false")
	elseif value_type == "number" then
		write_string(state, tostring(value))
	elseif value_type == "string" then
		serialize_string(state, value :: string)
	elseif value_type == "table" then
		if #(value :: {}) == 0 and next(value :: {}) ~= nil then
			serialize_table(state, value :: Object)
		else
			serialize_array(state, value :: Array)
		end
	else
		error("Unknown value", 2)
	end
end

-- deserialization

type DeserializerState = {
	src: string,
	cursor: number,
}

local function deserializer_error(state: DeserializerState, msg: string)
	return error(`JSON error - {msg} around {state.cursor}`)
end

local function skip_whitespace(state: DeserializerState): boolean
	state.cursor = string.find(state.src, "%S", state.cursor) :: number

	if not state.cursor then
		return false
	end

	return true
end

local function current_byte(state: DeserializerState)
	return string.byte(state.src, state.cursor)
end

local function deserialize_number(state: DeserializerState)
	-- first "segment"
	local n_start, n_end = string.find(state.src, "^[%-%deE]*", state.cursor)

	if not n_start then
		-- i dont think this is possible
		deserializer_error(state, "Could not match a number literal?")
	end

	if string.byte(state.src, n_end :: number + 1) == string.byte(".") then -- decimal!
		local dec_start, dec_end = string.find(state.src, "^[eE%-+%d]+", n_end :: number + 2)

		if not dec_start then
			deserializer_error(state, "Trailing '.' in number value")
		end

		n_end = dec_end
	end

	local num = tonumber(string.sub(state.src, n_start :: number, n_end))

	if not num then
		deserializer_error(state, "Malformed number value")
	end

	state.cursor = n_end :: number + 1

	return num
end

local ESCAPE_REVERSE = {
	["b"] = "\b",
	["f"] = "\f",
	["n"] = "\n",
	["r"] = "\r",
	["t"] = "\t",
}

local PATTERN = "([^\\]?)\\([bfnrt])"
local function unescape(before: string, code: string): string
	return before .. assert(ESCAPE_REVERSE[code], "Invalid escape code passed to unescape")
end

local function deserialize_string(state: DeserializerState): string
	state.cursor += 1

	local start_pos = state.cursor

	if current_byte(state) == string.byte('"') then
		state.cursor += 1

		return ""
	end

	while state.cursor <= #state.src do
		if current_byte(state) == string.byte('"') then
			state.cursor += 1

			return (string.gsub(string.sub(state.src, start_pos, state.cursor - 2), PATTERN, unescape :: any))
		end

		if current_byte(state) == string.byte("\\") then
			state.cursor += 1
		end

		state.cursor += 1
	end

	-- error

	state.cursor = start_pos

	return deserializer_error(state, "Unterminated string")
end

local deserialize

local function deserialize_array(state: DeserializerState): Array
	state.cursor += 1

	local current: Array = {}

	local expecting_value = false
	while state.cursor < #state.src do
        skip_whitespace(state)

		if current_byte(state) == string.byte(",") then
			expecting_value = true
			state.cursor += 1
		end
        
        skip_whitespace(state)

		if current_byte(state) == string.byte("]") then
			break
		end				

		table.insert(current, deserialize(state))

		expecting_value = false
	end

	if expecting_value then
		deserializer_error(state, "Trailing comma")
	end

	if not skip_whitespace(state) or current_byte(state) ~= string.byte("]") then
		deserializer_error(state, "Unterminated array")
	end

	state.cursor += 1

	return current
end

local function deserialize_object(state: DeserializerState): Object
	state.cursor += 1

	local current = {}

	local expecting_value = false
	while state.cursor < #state.src do
		skip_whitespace(state)

		if current_byte(state) == string.byte("}") then
			break
		end

		skip_whitespace(state)

		if current_byte(state) ~= string.byte("\"") then
			return deserializer_error(state, "Expected a string key")
		end

		local key = deserialize_string(state)

        skip_whitespace(state)

		if current_byte(state) ~= string.byte(":") then
			return deserializer_error(state, "Expected ':' for key value pair")
		end

		state.cursor += 1

		local value = deserialize(state)

		current[key] = value

		if not skip_whitespace(state) then
			deserializer_error(state, "Unterminated object")
		end

        skip_whitespace(state)

		if current_byte(state) == string.byte(",") then
			expecting_value = true
			state.cursor += 1
		else
			expecting_value = false
		end
	end

	if expecting_value then
		return deserializer_error(state, "Trailing comma")
	end
	
	if not skip_whitespace(state) or current_byte(state) ~= string.byte("}") then
		deserializer_error(state, "Unterminated object")
	end

	state.cursor += 1

	return current
end

deserialize = function(state: DeserializerState): Value
	skip_whitespace(state)

	local four_chars = string.sub(state.src, state.cursor, state.cursor + 3)

	if four_chars == "null" then
		state.cursor += 4
		return json.NULL
	elseif four_chars == "true" then
		state.cursor += 4
		return true
	elseif string.sub(state.src, state.cursor, state.cursor + 4) == "false" then
		state.cursor += 5
		return false
	elseif string.match(state.src, "^[%d%.]", state.cursor) then
		-- number
		return deserialize_number(state)
	elseif string.byte(state.src, state.cursor) == string.byte('"') then
		return deserialize_string(state)
	elseif string.byte(state.src, state.cursor) == string.byte("[") then
		return deserialize_array(state)
	elseif string.byte(state.src, state.cursor) == string.byte("{") then
		return deserialize_object(state)
	else		
		deserializer_error(state, `Unexpected token '{string.sub(state.src, state.cursor, state.cursor)}'`)
	end

	error("idk")
end

-- user-facing

json.serialize = function(value: Value, prettyPrint: boolean?)
	local state: SerializerState = {
		buf = buffer.create(1024),
		cursor = 0,
		prettyPrint = prettyPrint or false,
		depth = 0,
	}

	serialize_any(state, value)

	return buffer.readstring(state.buf, 0, state.cursor)
end

json.deserialize = function(src: string)
	local state = {
		src = src,
		cursor = 0,
	}

	return deserialize(state)
end

function json.dump_instrument()
	print(`deserialize_number - {deserialize_number_count.count}s`)
end

return table.freeze(json)
