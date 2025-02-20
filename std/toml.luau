--!strict

local toml = {}

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

local function serialize(tbl: {}): string
    local state: SerializerState = {
        buf = buffer.create(1024),
        cursor = 0,
    }

    for key, value in tbl do
        if typeof(value) == "table" and next(value) ~= nil and #value == 0 then
            write_string(state, "[" .. key .. "]\n")
            for k, v in value do
                write_string(state, k .. " = " .. tostring(v) .. "\n")
            end
        else
            write_string(state, key .. " = " .. tostring(value) .. "\n")
        end
    end

    return buffer.readstring(state.buf, 0, state.cursor)
end

toml.serialize = serialize

return toml