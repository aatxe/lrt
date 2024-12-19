local task = require("./task")
-- blocking 
local f = fs.open("temp", "w+")
local x = "This is a string I am writing to a file"
fs.write(f, x)
fs.close(f)

local t = task.create(function()
    return fs.readasync("temp")
end)

local t2 = task.await(t)
print(t2)
print(t2 == x)


