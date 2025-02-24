local fs = require("@lute/fs")
local luau = require("@lute/luau")

local pp = require("@batteries/pp")

local function select<T>(node, predicate: ({[string]: any}) -> T?): { T }
    local nodes = {}

    local function helper(n)
        if typeof(n) ~= "table" then return end

        local result = predicate(n)
        if result ~= nil then
            table.insert(nodes, result)
        end

        for key, value in (n :: { unknown }) do
            if key == "tag" or key == "location" then continue end

            helper(value)
        end
    end

    helper(node)
    return nodes
end

local path = tostring(...)
local source = fs.readfiletostring(path)

local prog = luau.parse(source)

local function lintForDivZeroByZero(prog)
    local function unwrapParens(n)
        if n.tag == "group" then
            return unwrapParens(n.expression)
        end
        return n
    end

    local function isZero(n)
        return n.tag == "number" and n.value == 0
    end

    local function isZeroDivZero(n)
        return n.tag == "binary" and
            n.operator == "/" and
            isZero(unwrapParens(n.lhsoperand)) and
            isZero(unwrapParens(n.rhsoperand))
    end

    local function isComparisonToZeroZero(n)
        if not (n.operator == "~=" or n.operator == "==") then
            return nil
        end

        local lhs = unwrapParens(n.lhsoperand)
        local rhs = unwrapParens(n.rhsoperand)

        if isZeroDivZero(lhs) then return rhs end
        if isZeroDivZero(rhs) then return lhs end

        return nil
    end

    local allBinaryOperators = select(prog, function(n)
        return if n.tag == "binary" then n else nil
    end)

    local violations = {}
    for _, node in allBinaryOperators do
        local expr = isComparisonToZeroZero(node)
        if expr ~= nil then
            local op_as_string = node.operator
            table.insert(violations, {
                severity = "err",
                node = node,
                message = `Don't compare things to 0/0, try: expr {op_as_string} expr`
            })
        end
    end

    return violations
end

print(pp(lintForDivZeroByZero(prog)))
