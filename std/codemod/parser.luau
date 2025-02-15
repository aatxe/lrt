--!strict

local luau = require("@lute/luau")

type Position = {
	line: number,
	column: number,
}

type Location = {
	begin: Position,
	["end"]: Position, -- TODO: do we really want to have to use brackets everywhere?
}

type Whitespace = {
	tag: "whitespace",
	location: Location,
	text: string,
}

-- type SingleLineComment = {
-- 	tag: "single_line_comment",
-- 	location: Location,
-- 	text: string,
-- }

-- type MultiLineComment = {
-- 	tag: "multi_line_comment",
-- 	location: Location,
-- 	text: string,
-- 	depth: number,
-- }

type Trivia = Whitespace

type Token<Kind = string> = {
	read leadingTrivia: { Trivia },
	read position: Position,
	read text: Kind,
	read trailingTrivia: { Trivia },
}

type AstExprConstantString = {
	tag: "string",
	value: string,
	quoteStyle: "single" | "double" | "block" | "interp",
	blockDepth: number,
}

type AstExprGlobal = {
	tag: "global",
	name: Token,
}

type AstExprCall = {
	tag: "call",
	func: AstExpr, -- TODO: stricter?
	arguments: { AstExpr },
	openParens: Token<"(">?,
	commas: { Token<","> },
	closeParens: Token<")">?,
}

type AstExprIndexName = {
	tag: "indexname",
	expr: AstExpr,
	index: string,
	indexLocation: Location,
	accessor: {
		position: Position,
		value: "." | ":",
	},
}

type AstExprIndexExpr = {
	tag: "index",
	expr: AstExpr,
	index: AstExpr,
}

type AstExprTableItem =
	| { kind: "list", value: AstExpr }
	| { kind: "record", key: string, equals: Token<"=">, value: AstExpr }
	| { kind: "general", key: string, equals: Token<"=">, value: AstExpr }

type AstExprTable = {
	tag: "table",
	openBrace: Token<"{">,
	entries: { never },
	closeBrace: Token<"}">,
}

type AstExpr = AstExprConstantString | AstExprGlobal | AstExprCall | AstExprIndexName | AstExprIndexExpr | AstExprTable

type AstStatBlock = {
	tag: "block",
	statements: { AstStat },
}

type AstStatReturn = {
	tag: "return",
	["return"]: Token<"return">,
	expressions: { AstExpr },
	commas: { Token<","> },
}

type AstStatExpr = {
	tag: "expression",
	expression: AstExpr,
}

type AstStat = AstStatBlock | AstStatReturn | AstStatExpr

local function exhaustiveMatch(value: never): never
	error(`Unknown value in exhaustive match: {value}`)
end

--- Parses Luau source code into an AstStatBlock
local function parse(source: string): AstStatBlock
	return luau.parse(source).root
end

--- Returns a string representation of an AstStatBlock
local function printBlock(block: AstStatBlock): string
	local result = ""
	for _, statement in block.statements do
		result ..= printStatement(statement)
	end
	return result
end

local function printTrivia(trivia: Trivia): string
	if trivia.tag == "whitespace" then
		return trivia.text
	else
		return exhaustiveMatch(trivia.tag)
	end
end

local function printToken(token: Token): string
	local result = ""

	for _, trivia in token.leadingTrivia do
		result ..= printTrivia(trivia)
	end

	result ..= token.text

	for _, trivia in token.trailingTrivia do
		result ..= printTrivia(trivia)
	end

	return result
end

local function printCommaSeparatedList<T>(list: { T }, commas: { Token<","> }, printer: (T) -> string): string
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

local function printExpression(expr: AstExpr): string
	if expr.tag == "string" then
		if expr.quoteStyle == "single" then
			return `'{expr.value}'`
		elseif expr.quoteStyle == "double" then
			return `"{expr.value}"`
		elseif expr.quoteStyle == "block" then
			local equals = string.rep("=", expr.blockDepth)
			return `[{equals}[{expr.value}]{equals}]`
		elseif expr.quoteStyle == "interp" then
			return "`" .. expr.value .. "`"
		else
			return exhaustiveMatch(expr.quoteStyle)
		end
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

function printStatement(stmt: AstStat): string
	if stmt.tag == "block" then
		return printBlock(stmt)
	elseif stmt.tag == "expression" then
		return printExpression(stmt.expression)
	elseif stmt.tag == "return" then
		return printToken(stmt["return"]) .. printCommaSeparatedList(stmt.expressions, stmt.commas, printExpression)
	else
		return exhaustiveMatch(stmt.tag)
	end
end

return {
	parse = parse,
	print = printBlock,
}
