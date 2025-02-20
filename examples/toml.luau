local toml = require("@std/toml")

local serialized = toml.serialize({
	["hello"] = "world",
	["world"] = {
		["v1"] = "hi",
		["v2"] = 8080,
		["v3"] = true,
	},
	["users"] = {
		[1] = {
			["name"] = "Alice",
			["age"] = 25,
		},
		[2] = {
			["name"] = "Bob",
			["age"] = 30,
		}
	},
})

print(serialized)

local deserialized = toml.deserialize([=[
hello = "world"

[world]
v1 = "hi"
v2 = 8080
v3 = true

[[users]]
name = "Alice"
age = 25

[[users]]
name = "Bob"
age = 30
]=])

print(deserialized)