local vm = require("@lrt/vm")

local task = require("@std/task")

local function getslice(t, n, m)
    local size = math.ceil(#t / m)

    local slice = table.create(size)
    table.move(t, 1 + (n - 1) * size, n * size, 1, slice)
    return slice
end

local function merge(t1, t2, comp)
    local t = table.create(#t1 + #t2)
    local pos1, pos2, pos = 1, 1, 1

    while pos1 < #t1 and pos2 < #t2 do
        if comp(t1[pos1], t2[pos2]) then
            t[pos] = t1[pos1]
            pos += 1
            pos1 += 1
        else
            t[pos] = t2[pos2]
            pos += 1
            pos2 += 1
        end
    end

    while pos1 < #t1 do
        t[pos] = t1[pos1]
        pos += 1
        pos1 += 1
    end

    while pos2 < #t2 do
        t[pos] = t2[pos2]
        pos += 1
        pos2 += 1
    end

    return t
end

local threadCount = 8
local threads = {}

for i = 1, threadCount do
    table.insert(threads, vm.spawn("./parallel_sort_helper"))
end

local function parallelMergeSort(t, comp)
    local slices = {}

    for i = 1, threadCount do
        table.insert(slices, task.create(threads[i].sort, getslice(t, i, threadCount)))
    end

    local tomerge = table.pack(task.awaitall(table.unpack(slices)))
    tomerge.n = nil

    while #tomerge > 1 do
        local next = {}
        for i = 1, #tomerge, 2 do
            table.insert(next, merge(tomerge[i], tomerge[i + 1], comp))
        end

        tomerge = next
    end

    return tomerge[1]
end

local size = 1_000_000

local t = table.create(size, 0)

for i in t do
    t[i] = math.random(1, 10000000)
end

local t2 = table.clone(t)

local startseq = os.clock()
table.sort(t, function(a, b) return a < b end)
print("sequential in:", os.clock() - startseq)

local startparallel = os.clock()
t2 = parallelMergeSort(t2, function(a, b) return a < b end)
print("parallel in:", os.clock() - startparallel)

print("part for comparison")

for i = #t / 2 - 5, #t / 2 + 5 do
    print(t[i], t2[i])
end
