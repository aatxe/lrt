local vm = require("@lrt/vm")

local task = require("@std/task")

print("start")

local vm1 = vm.spawn("./fib")
local vm2 = vm.spawn("./fib")
local vm3 = vm.spawn("./fib")

print("fib(15):", vm1.fib(15))
print("fib(20):", vm2.fib(20))
print("done inline")

do
    local start = os.clock();

    task.await(task.create(function()
        print("fib(10):", vm1.fib(10))
        print("fib(20):", vm1.fib(20))
        print("fib(33):", vm1.fib(33))
    end))

    print("multiple direct fib in", os.clock() - start)
end

do
    local start = os.clock();

    print("fib(33):", task.await(task.create(vm1.fib, 33)))

    print("single task fib in", os.clock() - start)
end

do
    local start = os.clock();

    print("fib(33):", task.await(task.create(vm1.fibTable, { n = 33 })))

    print("single task fibTable in", os.clock() - start)
end

do
    local start = os.clock();

    local s1, s2, s3 = task.create(vm1.fib, 33), task.create(vm1.fib, 33), task.create(vm1.fib, 33)

    print("fib(33):", task.awaitall(s1, s2, s3))

    print("3 serial fibs in", os.clock() - start)
end

do
    local start = os.clock();

    local p1, p2, p3 = task.create(vm1.fib, 33), task.create(vm2.fib, 33), task.create(vm3.fib, 33)

    print("fib(33):", task.awaitall(p1, p2, p3))

    print("3 parallel fibs in", os.clock() - start)
end

print("completed")
