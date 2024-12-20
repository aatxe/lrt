local pretty = require("@std/pp")
local foo = luau.parseexpr("{} + 2")
print(pretty(foo))
