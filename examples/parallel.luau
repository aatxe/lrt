local module = {}

function module.fib(n: number)
    return if n <= 2 then 1 else module.fib(n - 1) + module.fib(n - 2)
end

return module
