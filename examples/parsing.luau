local luau = require("@lrt/luau")

local pretty = require("@std/pp")

local foo = luau.parseexpr("5")
print(pretty(foo))
