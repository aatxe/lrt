--!strict

local luau = require("@lute/luau")
local T = require("./ast_types")

--- Parses Luau source code into an AstStatBlock
local function parse(source: string): T.AstStatBlock
	return luau.parse(source).root
end

return {
	parse = parse,
}
