local time = require("@std/time")
local Duration = time.Duration

local fiveSeconds = Duration.fromSecs(5)
local tenSeconds = fiveSeconds * 2
local fifteenSeconds = fiveSeconds + tenSeconds

print(fiveSeconds)
print(tenSeconds)
print(fifteenSeconds)

print(fiveSeconds < tenSeconds)
print (fifteenSeconds > tenSeconds)

local randomNanos = Duration.fromNanos(25038)
print(randomNanos)

local randomMillis = Duration.fromMillis(25038)
print(randomMillis)

print(randomMillis < randomNanos)
print(randomMillis + randomNanos > fiveSeconds)
