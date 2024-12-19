local PrettyPrint = require("./PrettyPrint")
local foo = luau.parseexpr("{} + 2")
print(PrettyPrint(foo))
