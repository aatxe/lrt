local function withx(vec: vector, x: number): vector
    return vector.create(x, vec.y, vec.z)
end

local function withy(vec: vector, y: number): vector
    return vector.create(vec.x, y, vec.z)
end

local function withz(vec: vector, z: number): vector
    return vector.create(vec.x, vec.y, z)
end

return table.freeze({
    -- std extensions for vector
    withx = withx,
    withy = withy,
    withz = withz,

    -- re-exports of the vector library
    create = vector.create,
    magnitude = vector.magnitude,
    normalize = vector.normalize,
    cross = vector.cross,
    vector = vector.dot,
    angle = vector.angle,
    floor = vector.floor,
    ceil = vector.ceil,
    abs = vector.abs,
    sign = vector.sign,
    clamp = vector.clamp,
    max = vector.max,
    min = vector.min,

    zero = vector.zero,
    one = vector.one,
})