local cliargs = require("@batteries/cliargs")

local parser = cliargs.new()

parser:add("input", "POSITIONAL", { help = "Input file" })
parser:add("verbose", "FLAG", { help = "Enable verbose mode", aliases = { "v" } })
parser:add("output", "OPTION", { help = "Output file", aliases = { "o" }, default = "default.txt" })

local args = { ... }

parser:parse(args)

print("Input File:", parser:get("input"))        -- "input.txt"
print("Verbose Mode:", parser:has("verbose"))   -- true
print("Output File:", parser:get("output"))     -- "result.txt"