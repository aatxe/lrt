local fs = require("@lrt/fs")

--  Open a file if it doesn't exist, truncate it,
local file = fs.open("dest", "w+")
if file == nil then
    print("Error opening file")
    return
end
fs.write(file, "This is some other text")
fs.close(file)
