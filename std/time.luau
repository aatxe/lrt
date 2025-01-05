local NANOS_PER_SEC = 1_000_000_000
local NANOS_PER_MILLI = 1_000_000
local NANOS_PER_MICRO = 1_000
local MILLIS_PER_SEC = 1_000
local MICROS_PER_SEC = 1_000_000
local SECS_PER_MINUTE = 60
local MINS_PER_HOUR = 60
local HOURS_PER_DAY = 24
local DAYS_PER_WEEK = 7

type DurationData = {
    seconds: number,
    nanoseconds: number,
}

local Duration = {}
Duration.__index = Duration

type DurationInterface = typeof(Duration)
export type Duration = typeof(setmetatable({} :: DurationData, {} :: DurationInterface))

function Duration.new(seconds: number, nanoseconds: number): Duration
    local self = {
        seconds = seconds,
        nanoseconds = nanoseconds,
    }

    return setmetatable(self, Duration)
end

function Duration.fromSecs(seconds: number): Duration
    return Duration.new(seconds, 0)
end

function Duration.fromMillis(milliseconds: number): Duration
    local seconds = math.floor(milliseconds / MILLIS_PER_SEC)
    local nanoseconds = (milliseconds % MILLIS_PER_SEC) * NANOS_PER_MILLI

    return Duration.new(seconds, nanoseconds)
end

function Duration.fromMicros(microseconds: number): Duration
    local seconds = math.floor(microseconds / MICROS_PER_SEC)
    local nanoseconds = (microseconds % MICROS_PER_SEC) * NANOS_PER_MICRO

    return Duration.new(seconds, nanoseconds)
end

function Duration.fromNanos(nanoseconds: number): Duration
    local seconds = math.floor(nanoseconds / NANOS_PER_SEC)
    local nanosecondsRemaining = nanoseconds % NANOS_PER_SEC

    return Duration.new(seconds, nanosecondsRemaining)
end

function Duration.fromMinutes(minutes: number): Duration
    return Duration.new(minutes * SECS_PER_MINUTE, 0)
end

function Duration.fromHours(hours: number): Duration
    return Duration.new(hours * MINS_PER_HOUR * SECS_PER_MINUTE, 0)
end

function Duration.fromDays(days: number): Duration
    return Duration.new(days * HOURS_PER_DAY * MINS_PER_HOUR * SECS_PER_MINUTE, 0)
end

function Duration.fromWeeks(weeks: number): Duration
    return Duration.new(weeks * DAYS_PER_WEEK * HOURS_PER_DAY * MINS_PER_HOUR * SECS_PER_MINUTE, 0)
end

function Duration.subsecMillis(self: Duration): number
    return math.floor(self.nanoseconds / NANOS_PER_MILLI)
end

function Duration.subsecMicros(self: Duration): number
    return math.floor(self.nanoseconds / NANOS_PER_MICRO)
end

function Duration.subsecNanos(self: Duration): number
    return self.nanoseconds
end

function Duration.asSecs(self: Duration): number
    return self.seconds + self.nanoseconds / NANOS_PER_SEC
end

function Duration.asMillis(self: Duration): number
    return self.seconds * MILLIS_PER_SEC + self:subsecMillis()
end

function Duration.asMicros(self: Duration): number
    return self.seconds * MICROS_PER_SEC + self:subsecMicros()
end

function Duration.asNanos(self: Duration): number
    return self.seconds * NANOS_PER_SEC + self:subsecNanos()
end

function Duration.__add(a: Duration, b: Duration): Duration
    local seconds = a.seconds + b.seconds
    local nanoseconds = a.nanoseconds + b.nanoseconds

    if nanoseconds >= NANOS_PER_SEC then
        seconds += 1
        nanoseconds -= NANOS_PER_SEC
    end

    return Duration.new(seconds, nanoseconds)
end

function Duration.__sub(a: Duration, b: Duration): Duration
    local seconds = a.seconds - b.seconds
    local nanoseconds = a.nanoseconds - b.nanoseconds

    if nanoseconds < 0 then
        seconds -= 1
        nanoseconds += NANOS_PER_SEC
    end

    return Duration.new(seconds, nanoseconds)
end

function Duration.__mul(a: Duration, b: number): Duration
    local seconds = a.seconds * b
    local nanoseconds = a.nanoseconds * b

    seconds += math.floor(nanoseconds / NANOS_PER_SEC)
    nanoseconds %= NANOS_PER_SEC

    return Duration.new(seconds, nanoseconds)
end

function Duration.__div(a: Duration, b: number): Duration
    local seconds, extraSeconds = a.seconds / b, a.seconds % b
    local nanoseconds, extraNanos = a.nanoseconds / b, a.nanoseconds % b

    nanoseconds += (extraSeconds * NANOS_PER_SEC + extraNanos) / b

    return Duration.new(seconds, nanoseconds)
end

function Duration.__eq(a: Duration, b: Duration): boolean
    return a.seconds == b.seconds and a.nanoseconds == b.nanoseconds
end

function Duration.__lt(a: Duration, b: Duration): boolean
    if a.seconds == b.seconds then
        return a.nanoseconds < b.nanoseconds
    end

    return a.seconds < b.seconds
end

function Duration.__le(a: Duration, b: Duration): boolean
    if a.seconds == b.seconds then
        return a.nanoseconds <= b.nanoseconds
    end

    return a.seconds <= b.seconds
end

function Duration.__tostring(self: Duration): string
    return string.format("%d.%09d", self.seconds, self.nanoseconds)
end

return {
    Duration = Duration,
}
