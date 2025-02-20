local toml = {}

-- serialization

type SerializerState = {
	buf: buffer,
	cursor: number,
}

local function check_state(state: SerializerState, len: number)
	local cur_len = buffer.len(state.buf)
	if state.cursor + len >= cur_len then
		local new_buffer = buffer.create(cur_len * 2)
		buffer.copy(new_buffer, 0, state.buf)
		state.buf = new_buffer
	end
end

local function write_string(state: SerializerState, str: string)
	check_state(state, string.len(str))
	buffer.writestring(state.buf, state.cursor, str)
	state.cursor += string.len(str)
end

local function serialize_value(value)
	if typeof(value) == "string" then
		return "\"" .. value .. "\""
	else
		return tostring(value)
	end
end

local function serialize(tbl: {}): string
	local state: SerializerState = {
		buf = buffer.create(1024),
		cursor = 0,
	}

	for key, value in pairs(tbl) do
		if typeof(value) == "table" then
			if #value > 0 then
				for _, entry in ipairs(value) do
					write_string(state, "[[" .. key .. "]]\n")
					for k, v in pairs(entry) do
						write_string(state, k .. " = " .. serialize_value(v) .. "\n")
					end
				end
			elseif next(value) ~= nil then
				write_string(state, "[" .. key .. "]\n")
				for k, v in pairs(value) do
					write_string(state, k .. " = " .. serialize_value(v) .. "\n")
				end
			end
		else
			write_string(state, key .. " = " .. serialize_value(value) .. "\n")
		end
	end

	return buffer.readstring(state.buf, 0, state.cursor)
end

-- deserialization

type DeserializerState = {
	buf: string,
	cursor: number,
}

local function skip_whitespace(state: DeserializerState)
	local pos = state.cursor
	while pos <= string.len(state.buf) and string.match(string.sub(state.buf, pos, pos), "%s") do
		pos += 1
	end
	state.cursor = pos
end

local function read_line(state: DeserializerState)
	local next_line = string.find(state.buf, "\n", state.cursor) or string.len(state.buf) + 1
	local line = string.sub(state.buf, state.cursor, next_line - 1)
	state.cursor = next_line + 1
	return line
end

local function deserialize(input: string)
	local state: DeserializerState = {
		buf = input,
		cursor = 1,
	}
	local result = {}
	local currentTable = result
	local arrayTables = {}

	while state.cursor <= string.len(state.buf) do
		skip_whitespace(state)
		local line = read_line(state)

		if line == "" or string.sub(line, 1, 1) == "#" then
			-- ignore comments and empty lines
		elseif string.match(line, "^%[%[(.-)%]%]$") then
			local tableName = string.match(line, "^%[%[(.-)%]%]$")
			arrayTables[tableName] = arrayTables[tableName] or {}

			local newEntry = {}
			table.insert(arrayTables[tableName], newEntry)

			result[tableName] = arrayTables[tableName]
			currentTable = newEntry
		elseif string.match(line, "^%[(.-)%]$") then
			local tableName = string.match(line, "^%[(.-)%]$")
			result[tableName] = result[tableName] or {}
			currentTable = result[tableName]
		elseif string.match(line, "^(.-)%s*=%s*(.-)$") then
			local key, value = string.match(line, "^(.-)%s*=%s*(.-)$")
			key = string.match(key, "^%s*(.-)%s*$")
			value = string.match(value, "^%s*(.-)%s*$")

			if string.match(value, '^"(.*)"$') or string.match(value, "^'(.*)'$") then
				value = string.sub(value, 2, -2)
			elseif tonumber(value) then
				value = tonumber(value)
			elseif value == "true" then
				value = true
			elseif value == "false" then
				value = false
			end

			currentTable[key] = value
		end
	end

	return result
end

-- user-facing

toml.serialize = serialize
toml.deserialize = deserialize

return toml