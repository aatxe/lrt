local task = require("@std/task")

local t = task.create(function()
    return net.getAsync("https://en.wikipedia.org/")
end)

print(task.await(t))

print(task.await(task.create(net.getAsync, "https://en.wikipedia.org/")))

local t1 = task.create(net.getAsync, "https://en.wikipedia.org/")
local t2 = task.create(net.getAsync, "https://www.google.com/")
local t3 = task.create(net.getAsync, "https://www.bing.com/")

-- print(task.awaitAll(t1, t2, t3))

local together = true
local start = os.clock()

if together then
    task.awaitAll(t1, t2, t3)
else
    task.await(t1)
    task.await(t2)
    task.await(t3)
end

print(os.clock() - start)

print("completed")
