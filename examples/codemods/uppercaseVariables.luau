--!strict
local codemod = require("@std/codemod")
local parser = require("@std/codemod/parser")
local printer = require("@std/codemod/printer")
local visitor = require("@std/codemod/visitor")

local T = require("@std/codemod/ast_types")

local function uppercaseVariables(ctx: codemod.Context)
	local cst = parser.parse(ctx.source)

	local makeUppercase = visitor.createVisitor()

	makeUppercase.visitLocal = function(node: T.AstLocal)
		node.name.text = node.name.text:upper()
		return false
	end

	makeUppercase.visitLocalReference = function(node: T.AstExprLocal)
		node.token.text = node.token.text:upper()
		return false
	end

	visitor.visitBlock(cst, makeUppercase)

	return printer.print(cst)
end

codemod.apply("examples/a.luau", uppercaseVariables)
