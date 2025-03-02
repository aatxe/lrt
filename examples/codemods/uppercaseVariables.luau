--!strict
local codemod = require("@batteries/codemod")
local parser = require("@batteries/codemod/parser")
local printer = require("@batteries/codemod/printer")
local visitor = require("@batteries/codemod/visitor")

local T = require("@batteries/codemod/ast_types")

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
