local toml = require("@batteries/toml")

local deserialized = toml.deserialize([=[
hello = "world"
infinity = inf
nan_value = nan
escaped_string = "This is a string with a newline\ncharacter"

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

[people.Tom]
age = 33
]=])

local serialized = toml.serialize(deserialized)

print(serialized)

print(deserialized)