local cliargs = require("@batteries/cliargs")

local parser = cliargs.new()

parser:add("input", "positional", { help = "Input file" })
parser:add("verbose", "flag", { help = "Enable verbose mode", aliases = { "v" } })
parser:add("output", "option", { help = "Output file", aliases = { "o" }, default = "default.txt" })

local args = { ... }

parser:parse(args)

print("Input File:", parser:get("input"))        -- "input.txt"
print("Verbose Mode:", parser:has("verbose"))   -- true
print("Output File:", parser:get("output"))     -- "result.txt"