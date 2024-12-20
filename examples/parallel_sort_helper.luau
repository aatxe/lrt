return {
    sort = function(t)
        table.sort(t, function(a, b) return a < b end)
        return t
    end,
}
