local toml = require("@std/toml")

local serialize = toml.serialize({
    hello = "world"
})

print(serialize)