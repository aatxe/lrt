--!strict
local codemod = require("@batteries/codemod")
local parser = require("@batteries/codemod/parser")
local printer = require("@batteries/codemod/printer")

local function identityTransform(ctx: codemod.Context)
	local cst = parser.parse(ctx.source)
	return printer.print(cst)
end

codemod.apply("examples/a.luau", identityTransform)
codemod.apply("examples/async_read.luau", identityTransform)
codemod.apply("examples/b.luau", identityTransform)
codemod.apply("examples/badisnan.luau", identityTransform)
codemod.apply("examples/json.luau", identityTransform)
codemod.apply("examples/main.luau", identityTransform)
codemod.apply("examples/net_example.luau", identityTransform)
codemod.apply("examples/parsing.luau", identityTransform)
codemod.apply("examples/time_example.luau", identityTransform)
codemod.apply("examples/writeFile.luau", identityTransform)
