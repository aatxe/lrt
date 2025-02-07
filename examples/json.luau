local json = require("@std/json")

local serialize = json.serialize({
    hello = "world"
}, true)

print(serialize)

local deserialized = json.deserialize([[{
    "hello": "world"
}]])

print(deserialized.hello)