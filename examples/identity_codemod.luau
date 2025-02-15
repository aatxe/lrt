--!strict
local codemod = require("@std/codemod")
local parser = require("@std/codemod/parser")
local printer = require("@std/codemod/printer")

local function identityTransform(ctx: codemod.Context)
	local cst = parser.parse(ctx.source)
	return printer.print(cst)
end

codemod.apply("examples/a.luau", identityTransform)
codemod.apply("examples/b.luau", identityTransform)
