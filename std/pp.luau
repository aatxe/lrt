local isPrimitiveType = {string = true, number = true, boolean = true}

local function isPrimitiveArray(array)
    local max, n = 0, 0

    for key, value in array do
        if typeof(key) ~= "number" then return false end
        if key <= 0 then return false end
        if not isPrimitiveType[typeof(value)] then return false end

        max = if key > max then key else max
        n = n + 1
    end

    return n == max
end

local function formatValue(value)
    if typeof(value) ~= "string" then
        return tostring(value)
    end

    return string.format("%q", value)
end

local function formatKey(key, seq)
    if seq then return "" end

    if typeof(key) ~= "string" then
        return `[{tostring(key)}] =`
    end

    -- key is a simple identifier
    if key:match("^[%a_][%w_]-$") == key then
        return `{key} = `
    end

    return `[{string.format("%q", key)}] = `
end

local typeSortOrder = {
    ["boolean"]  =  1,
    ["number"]   =  2,
    ["string"]   =  3,
    ["function"] =  4,
    ["vector"]   =  5,
    ["buffer"]   =  6,
    ["thread"]   =  7,
    ["table"]    =  8,
    ["userdata"] =  9,
    ["nil"]      = 10,
}

local function traverseTable(dataTable, seen, indent)
    local output = ""

    local indentStr = string.rep("\t", indent)

    local keys = {}

    for key, value in dataTable do
        if isPrimitiveType[typeof(key)] then
            keys[#keys + 1] = key
        end
    end

    table.sort(keys, function(a,b)
        local typeofTableA, typeofTableB = typeof(dataTable[a]), typeof(dataTable[b])

        if typeofTableA ~= typeofTableB then
            return typeSortOrder[typeofTableA] < typeSortOrder[typeofTableB]
        end

        if typeof(a) == "number" and typeof(b) == "number" then
            return a < b
        end

        return tostring(a) < tostring(b)
    end)

    local inSequence = false
    local previousKey = 0

    for idx, key in keys do
        if typeof(key) == "number" and key > 0 and key - 1 == previousKey then
            previousKey = key
            inSequence = true
        else
            inSequence = false
        end

        local value = dataTable[key]
        if typeof(value) ~= 'table' then
            if isPrimitiveType[typeof(value)] then
                output = `{output}{indentStr}{formatKey(key, inSequence)}{formatValue(value)},\n`
            end

            continue
        end

        -- prevents self-referential tables from looping infinitely
        if seen[value] then
            continue
        end

        seen[value] = true

        local hasItems = false
        for key, val in value do
            if isPrimitiveType[typeof(key)] and isPrimitiveType[typeof(val)] then
                hasItems = true
                break
            end

            if typeof(val) == 'table' then
                hasItems = true
                break
            end
        end

        if not hasItems then
            output = string.format("%s%s%s{},\n", output, indentStr, formatKey(key, inSequence))
            continue
        end

        if isPrimitiveArray(value) then -- collapse primitive arrays
            output = string.format("%s%s%s{", output, indentStr, formatKey(key, inSequence))

            for index = 1, #value do
                output = output .. formatValue(value[index])
                if index < #value then
                    output = output .. ", "
                end
            end

            output = output .. "},\n"
            continue
        end

        output = string.format("%s%s%s{\n%s%s},\n",
                                output, indentStr, formatKey(key, inSequence),
                                traverseTable(value, seen, indent + 1), indentStr)

        seen[value] = nil
    end

    return output
end

return function(dataTable)
    return "{\n" .. traverseTable(dataTable, {[dataTable]=true}, 1) .. "}"
end
