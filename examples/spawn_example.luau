print("start")

local parallel1 = spawn("./parallel")
local parallel2 = spawn("./parallel")
local parallel3 = spawn("./parallel")

print(os.clock())

print(parallel1.fib(22))

print(os.clock())
