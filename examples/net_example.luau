local await = require("./await")

local t = await.task(function()
    return net.getAsync("https://en.wikipedia.org/")
end)

print(await.await(t))

print(await.await(await.task(net.getAsync, "https://en.wikipedia.org/")))

local t1 = await.task(net.getAsync, "https://en.wikipedia.org/")
local t2 = await.task(net.getAsync, "https://www.google.com/")
local t3 = await.task(net.getAsync, "https://www.bing.com/")

local together = false
local start = os.clock()

if together then
    await.awaitAll(t1, t2, t3)
else
    await.await(t1)
    await.await(t2)
    await.await(t3)
end

print(os.clock() - start)

-- print(await.awaitAll(t1, t2, t3))

print("completed")
