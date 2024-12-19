local module = {}

function module.fib(n: number)
    return if n <= 2 then 1 else module.fib(n - 1) + module.fib(n - 2)
end

function module.fibTable(t: { n: number })
    return if t.n <= 2 then 1 else module.fibTable({ n = t.n - 1}) + module.fibTable({ n = t.n - 2 })
end

return module
