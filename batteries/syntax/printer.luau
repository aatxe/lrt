--!strict
local T = require("./ast_types")
local visitor = require("./visitor")

local function exhaustiveMatch(value: never): never
	error(`Unknown value in exhaustive match: {value}`)
end

local function printTrivia(trivia: T.Trivia): string
	if trivia.tag == "whitespace" or trivia.tag == "comment" or trivia.tag == "blockcomment" then
		return trivia.text
	else
		return exhaustiveMatch(trivia.tag)
	end
end

local function printTriviaList(trivia: { T.Trivia })
	local result = ""
	for _, trivia in trivia do
		result ..= printTrivia(trivia)
	end
	return result
end

local function printToken(token: T.Token): string
	return printTriviaList(token.leadingTrivia) .. token.text .. printTriviaList(token.trailingTrivia)
end

local function printString(expr: T.AstExprConstantString): string
	local result = printTriviaList(expr.leadingTrivia)

	if expr.quoteStyle == "single" then
		result ..= `'{expr.text}'`
	elseif expr.quoteStyle == "double" then
		result ..= `"{expr.text}"`
	elseif expr.quoteStyle == "block" then
		local equals = string.rep("=", expr.blockDepth)
		result ..= `[{equals}[{expr.text}]{equals}]`
	elseif expr.quoteStyle == "interp" then
		result ..= "`" .. expr.text .. "`"
	else
		return exhaustiveMatch(expr.quoteStyle)
	end

	result ..= printTriviaList(expr.trailingTrivia)
	return result
end

--- Returns a string representation of an AstStatBlock
local function printBlock(block: T.AstStatBlock): string
	local printer = visitor.createVisitor()
	local result = ""

	printer.visitToken = function(node: T.Token)
		result ..= printToken(node)
		return false
	end

	printer.visitString = function(node: T.AstExprConstantString)
		result ..= printString(node)
		return false
	end

	visitor.visitBlock(block, printer)

	return result
end

return {
	print = printBlock,
}
