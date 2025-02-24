export type array<T> = {T}
export type dictionary<T> = {[string]: T}

local function map<K, A, B>(table: {[K]: A}, f: (A) -> B): {[K]: B}
    local new = {}

    for k, v in table do
        new[k] = f(v)
    end

    return new
end

local function filter<K, V>(table: {[K]: V}, predicate: (V) -> boolean): {[K]: V}
    local new = {}

    for k, v in table do
        if predicate(v) then
            new[k] = v
        end
    end

    return new
end

local function fold<K, V, A>(table: {[K]: V}, f: (A, V) -> A, initial: A): A
    local acc = initial

    for _, v in table do
        acc = f(acc, v)
    end

    return acc
end

return table.freeze({
    -- std extension for table
    map = map,
    filter = filter,
    fold = fold,

    -- re-exports of the table library
    clear = table.clear,
    clone = table.clone,
    concat = table.concat,
    create = table.create,
    find = table.find,
    freeze = table.freeze,
    insert = table.insert,
    isfrozen = table.isfrozen,
    maxn = table.maxn,
    move = table.move,
    pack = table.pack,
    remove = table.remove,
    sort = table.sort,
    unpack = table.unpack,
})