export type Task = {
    success: boolean?,
    result: any,
    co: thread
}

local function create(f, ...): Task
    local data = {}

    data.co = coroutine.create(function(...)
        local success, result = pcall(f, ...)

        data.success = success
        data.result = result
    end)

    coroutine.resume(data.co, ...)
    return data
end

local function await(t: Task)
    if not t.co then
        error(`await: argument 1 is not a task`)
    end

    while t.success == nil do
        defer()
    end

    if t.success then
        return t.result
    else
        error(t.result)
    end
end

local function awaitAll(...: Task)
    local tasks = table.pack(...)
    tasks.n = nil

    for i, v in tasks do
        if not v.co then
            error(`awaitAll: argument {i} is not a task`)
        end
    end

    local done = false

    while not done do
        done = true
        for i, v in tasks do
            if v.success == nil then
                done = false
                defer()
            end
        end
    end

    local results = {}

    for i, v in tasks do
        if v.success then
            table.insert(results, v.result)
        else
            error(v.result)
        end
    end

    return table.unpack(results)
end

return {
    create = create,
    await = await,
    awaitAll = awaitAll,
}
