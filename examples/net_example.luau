local task = require("./task")

local t = task.task(function()
    return net.getAsync("https://en.wikipedia.org/")
end)

print(task.await(t))

print(task.await(task.task(net.getAsync, "https://en.wikipedia.org/")))

local t1 = task.task(net.getAsync, "https://en.wikipedia.org/")
local t2 = task.task(net.getAsync, "https://www.google.com/")
local t3 = task.task(net.getAsync, "https://www.bing.com/")

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
