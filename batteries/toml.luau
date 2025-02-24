local toml = {}

-- serialization
type SerializerState = {
	buf: buffer,
	cursor: number,
}

local function serializeValue(value: string | number)
	if typeof(value) == "string" then
		value = string.gsub(value, '\\', '\\\\')
		value = string.gsub(value, '\n', '\\n')
		value = string.gsub(value, '\t', '\\t')
		return "\"" .. value .. "\""
	elseif value == math.huge then
		return "inf"
	elseif value == -math.huge then
		return "-inf"
	elseif value ~= value then
		return "nan"
	else
		return tostring(value)
	end
end

local function hasNestedTables(tbl: {})
	for _, v in tbl do
		if typeof(v) == "table" and next(v) ~= nil then
			return true
		end
	end
	return false
end

local function tableToToml(tbl: {}, parent: string)
	local result = ""
	local subTables = {}
	local hasDirectValues = false

	for k, v in tbl do
		if typeof(v) == "table" and next(v) ~= nil then
			if #v > 0 then
				for _, entry in v do
					result ..= "\n[[" .. (parent and parent .. "." or "") .. k .. "]]\n"
					result ..=  tableToToml(entry, nil)
				end
			else
				subTables[k] = v
			end
		else
			hasDirectValues = true
			result ..= k .. " = " .. serializeValue(v) .. "\n"
		end
	end

	for k, v in subTables do
		local key = parent and (parent .. "." .. k) or k
		
		if hasNestedTables(v) == true then
			result ..= tableToToml(v, key)
			continue
		end

		if hasDirectValues or parent then
			result ..= "\n[" .. key .. "]\n"
		end

		result ..= tableToToml(v, key)
	end

	return result
end


local function serialize(tbl: {}): string
	return tableToToml(tbl, nil)
end

-- deserialization
type DeserializerState = {
	buf: string,
	cursor: number,
}

local function skipWhitespace(state: DeserializerState)
	local pos = state.cursor
	while pos <= string.len(state.buf) and string.match(string.sub(state.buf, pos, pos), "%s") do
		pos += 1
	end
	state.cursor = pos
end

local function readLine(state: DeserializerState)
	local nextLine = string.find(state.buf, "\n", state.cursor) or string.len(state.buf) + 1
	local line = string.sub(state.buf, state.cursor, nextLine - 1)
	state.cursor = nextLine + 1
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
		skipWhitespace(state)
		local line = readLine(state)

		if line == "" or string.sub(line, 1, 1) == "#" then
			continue
		end

		if string.match(line, "^%[%[(.-)%]%]$") then
			local tableName = string.match(line, "^%[%[(.-)%]%]$")
			arrayTables[tableName] = arrayTables[tableName] or {}

			local newEntry = {}
			table.insert(arrayTables[tableName], newEntry)

			result[tableName] = arrayTables[tableName]
			currentTable = newEntry
		elseif string.match(line, "^%[(.-)%]$") then
			local tablePath = string.match(line, "^%[(.-)%]$")
			local parent = result

			for section in string.gmatch(tablePath, "([^.]+)") do
				if not parent[section] then parent[section] = {} end
				parent = parent[section]
			end

			currentTable = parent
		elseif string.match(line, "^(.-)%s*=%s*(.-)$") then
			local key, value = string.match(line, "^(.-)%s*=%s*(.-)$")
			key = string.match(key, "^%s*(.-)%s*$")
			value = string.match(value, "^%s*(.-)%s*$")

			if string.match(value, '^"(.*)"$') or string.match(value, "^'(.*)'$") then
				value = string.sub(value, 2, -2)
				value = string.gsub(value, '\\\\', '\\')
				value = string.gsub(value, '\\n', '\n')
				value = string.gsub(value, '\\t', '\t')
			elseif tonumber(value) then
				value = tonumber(value)
			elseif value == "true" then
				value = true
			elseif value == "false" then
				value = false
			elseif value == "inf" or value == "+inf" then
				value = math.huge
			elseif value == "-inf" then
				value = -math.huge
			elseif value == "nan" then
				value = 0/0
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