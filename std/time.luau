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

local duration = {}
duration.__index = duration

type DurationInterface = typeof(duration)
export type Duration = typeof(setmetatable({} :: DurationData, {} :: DurationInterface))

function duration.create(seconds: number, nanoseconds: number): Duration
    local self = {
        seconds = seconds,
        nanoseconds = nanoseconds,
    }

    return setmetatable(self, duration)
end

function duration.seconds(seconds: number): Duration
    return duration.create(seconds, 0)
end

function duration.milliseconds(milliseconds: number): Duration
    local seconds = math.floor(milliseconds / MILLIS_PER_SEC)
    local nanoseconds = (milliseconds % MILLIS_PER_SEC) * NANOS_PER_MILLI

    return duration.create(seconds, nanoseconds)
end

function duration.microseconds(microseconds: number): Duration
    local seconds = math.floor(microseconds / MICROS_PER_SEC)
    local nanoseconds = (microseconds % MICROS_PER_SEC) * NANOS_PER_MICRO

    return duration.create(seconds, nanoseconds)
end

function duration.nanoseconds(nanoseconds: number): Duration
    local seconds = math.floor(nanoseconds / NANOS_PER_SEC)
    local nanosecondsRemaining = nanoseconds % NANOS_PER_SEC

    return duration.create(seconds, nanosecondsRemaining)
end

function duration.minutes(minutes: number): Duration
    return duration.create(minutes * SECS_PER_MINUTE, 0)
end

function duration.hours(hours: number): Duration
    return duration.create(hours * MINS_PER_HOUR * SECS_PER_MINUTE, 0)
end

function duration.days(days: number): Duration
    return duration.create(days * HOURS_PER_DAY * MINS_PER_HOUR * SECS_PER_MINUTE, 0)
end

function duration.weeks(weeks: number): Duration
    return duration.create(weeks * DAYS_PER_WEEK * HOURS_PER_DAY * MINS_PER_HOUR * SECS_PER_MINUTE, 0)
end

function duration.subsecmillis(self: Duration): number
    return math.floor(self.nanoseconds / NANOS_PER_MILLI)
end

function duration.subsecmicros(self: Duration): number
    return math.floor(self.nanoseconds / NANOS_PER_MICRO)
end

function duration.subsecnanos(self: Duration): number
    return self.nanoseconds
end

function duration.toseconds(self: Duration): number
    return self.seconds + self.nanoseconds / NANOS_PER_SEC
end

function duration.tomilliseconds(self: Duration): number
    return self.seconds * MILLIS_PER_SEC + self:subsecmillis()
end

function duration.tomicroseconds(self: Duration): number
    return self.seconds * MICROS_PER_SEC + self:subsecmicros()
end

function duration.tonanoseconds(self: Duration): number
    return self.seconds * NANOS_PER_SEC + self:subsecnanos()
end

function duration.__add(a: Duration, b: Duration): Duration
    local seconds = a.seconds + b.seconds
    local nanoseconds = a.nanoseconds + b.nanoseconds

    if nanoseconds >= NANOS_PER_SEC then
        seconds += 1
        nanoseconds -= NANOS_PER_SEC
    end

    return duration.create(seconds, nanoseconds)
end

function duration.__sub(a: Duration, b: Duration): Duration
    local seconds = a.seconds - b.seconds
    local nanoseconds = a.nanoseconds - b.nanoseconds

    if nanoseconds < 0 then
        seconds -= 1
        nanoseconds += NANOS_PER_SEC
    end

    return duration.create(seconds, nanoseconds)
end

function duration.__mul(a: Duration, b: number): Duration
    local seconds = a.seconds * b
    local nanoseconds = a.nanoseconds * b

    seconds += math.floor(nanoseconds / NANOS_PER_SEC)
    nanoseconds %= NANOS_PER_SEC

    return duration.create(seconds, nanoseconds)
end

function duration.__div(a: Duration, b: number): Duration
    local seconds, extraSeconds = a.seconds / b, a.seconds % b
    local nanoseconds, extraNanos = a.nanoseconds / b, a.nanoseconds % b

    nanoseconds += (extraSeconds * NANOS_PER_SEC + extraNanos) / b

    return duration.create(seconds, nanoseconds)
end

function duration.__eq(a: Duration, b: Duration): boolean
    return a.seconds == b.seconds and a.nanoseconds == b.nanoseconds
end

function duration.__lt(a: Duration, b: Duration): boolean
    if a.seconds == b.seconds then
        return a.nanoseconds < b.nanoseconds
    end

    return a.seconds < b.seconds
end

function duration.__le(a: Duration, b: Duration): boolean
    if a.seconds == b.seconds then
        return a.nanoseconds <= b.nanoseconds
    end

    return a.seconds <= b.seconds
end

function duration.__tostring(self: Duration): string
    return string.format("%d.%09d", self.seconds, self.nanoseconds)
end

return {
    duration = duration,
}
