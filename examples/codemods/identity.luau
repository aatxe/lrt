--!strict
local codemod = require("@batteries/codemod")
local parser = require("@batteries/codemod/parser")
local printer = require("@batteries/codemod/printer")

local function identityTransform(ctx: codemod.Context)
	local cst = parser.parse(ctx.source)
	return printer.print(cst)
end

codemod.apply("examples/a.luau", identityTransform)
codemod.apply("examples/b.luau", identityTransform)
