--!strict
local T = require("./ast_types")

local function exhaustiveMatch(value: never): never
	error(`Unknown value in exhaustive match: {value}`)
end

--- Returns a string representation of an AstStatBlock
local function printBlock(block: T.AstStatBlock): string
	local result = ""
	for _, statement in block.statements do
		result ..= printStatement(statement)
	end
	return result
end

local function printTrivia(trivia: T.Trivia): string
	if trivia.tag == "whitespace" then
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

local function printLocal(node: T.AstLocal): string
	return printToken(node.name)
end

local function printCommaSeparatedList<T>(list: { T }, commas: { T.Token<","> }, printer: (T) -> string): string
	local result = ""

	for i, item in list do
		result ..= printer(item)

		if i ~= 1 then
			printToken(commas[i - 1])
		end
	end

	return result
end

local function printOptional<T>(item: T?, printer: (T) -> string): string
	if item then
		return printer(item)
	else
		return ""
	end
end

local function printExpression(expr: T.AstExpr): string
	if expr.tag == "string" then
		return printString(expr)
	elseif expr.tag == "global" then
		return printToken(expr.name)
	elseif expr.tag == "call" then
		return printExpression(expr.func)
			.. printOptional(expr.openParens, printToken)
			.. printCommaSeparatedList(expr.arguments, expr.commas, printExpression)
			.. printOptional(expr.closeParens, printToken)
	elseif expr.tag == "table" then
		-- TODO: print fields
		return printToken(expr.openBrace) .. printToken(expr.closeBrace)
	else
		return exhaustiveMatch(expr.tag)
	end
end

function printStatement(stmt: T.AstStat): string
	if stmt.tag == "block" then
		return printBlock(stmt)
	elseif stmt.tag == "expression" then
		return printExpression(stmt.expression)
	elseif stmt.tag == "return" then
		return printToken(stmt["return"]) .. printCommaSeparatedList(stmt.expressions, stmt.commas, printExpression)
	elseif stmt.tag == "local" then
		return printToken(stmt["local"])
			.. printCommaSeparatedList(stmt.variables, stmt.variablesCommas, printLocal)
			.. printOptional(stmt.equals, printToken)
			.. printCommaSeparatedList(stmt.values, stmt.valuesCommas, printExpression)
	else
		return exhaustiveMatch(stmt.tag)
	end
end

return {
	print = printBlock,
}
