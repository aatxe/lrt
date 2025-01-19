local time = require("@std/time")
local duration = time.duration

local fiveSeconds = duration.seconds(5)
local tenSeconds = fiveSeconds * 2
local fifteenSeconds = fiveSeconds + tenSeconds

print(fiveSeconds)
print(tenSeconds)
print(fifteenSeconds)

print(fiveSeconds < tenSeconds)
print (fifteenSeconds > tenSeconds)

local randomNanos = duration.nanoseconds(25038)
print(randomNanos)

local randomMillis = duration.milliseconds(25038)
print(randomMillis)

print(randomMillis < randomNanos)
print(randomMillis + randomNanos > fiveSeconds)
