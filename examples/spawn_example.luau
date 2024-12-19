local task = require("./task")

print("start")

local vm1 = spawn("./fib")
local vm2 = spawn("./fib")
local vm3 = spawn("./fib")

do
    local start = os.clock();

    task.await(task.create(function()
        print(vm1.fib(10))
        print(vm1.fib(20))
        print(vm1.fib(33))
    end))

    print("multiple direct fib in", os.clock() - start)
end

do
    local start = os.clock();

    print(task.await(task.create(vm1.fib, 33)))

    print("single task fib in", os.clock() - start)
end

do
    local start = os.clock();

    print(task.await(task.create(vm1.fibTable, { n = 33 })))

    print("single task fibTable in", os.clock() - start)
end

do
    local start = os.clock();

    local s1, s2, s3 = task.create(vm1.fib, 33), task.create(vm1.fib, 33), task.create(vm1.fib, 33)

    print(task.awaitAll(s1, s2, s3))

    print("3 serial fibs in", os.clock() - start)
end

do
    local start = os.clock();

    local p1, p2, p3 = task.create(vm1.fib, 33), task.create(vm2.fib, 33), task.create(vm3.fib, 33)

    print(task.awaitAll(p1, p2, p3))

    print("3 parallel fibs in", os.clock() - start)
end

print("completed")
