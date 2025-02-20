local toml = require("@std/toml")

local serialize = toml.serialize({
    ["hello"] = "world",
	["world"] = {
		["v1"] = "hello",
		["v2"] = 8080,
		["v3"] = true,
	},
})

print(serialize)