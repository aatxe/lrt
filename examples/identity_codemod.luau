--!strict
local codemod = require("@std/codemod")
local parser = require("@std/codemod/parser")

local function identityTransform(ctx: codemod.Context)
	local cst = parser.parse(ctx.source)
	return parser.print(cst)
end

codemod.apply("examples/b.luau", identityTransform)
