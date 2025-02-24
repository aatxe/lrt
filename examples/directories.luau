local fs = require("@lute/fs")

for _, file in fs.listdir("./examples") do
    print(`Example {file.name} is a {file.type}`)
end
