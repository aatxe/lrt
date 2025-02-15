export type Position = {
	line: number,
	column: number,
}

export type Location = {
	begin: Position,
	["end"]: Position, -- TODO: do we really want to have to use brackets everywhere?
}

export type Whitespace = {
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

export type Trivia = Whitespace

export type Token<Kind = string> = {
	read leadingTrivia: { Trivia },
	read position: Position,
	read text: Kind,
	read trailingTrivia: { Trivia },
}

export type AstLocal = {
	name: Token<string>,
}

export type AstExprConstantString = Token<string> & {
	tag: "string",
	quoteStyle: "single" | "double" | "block" | "interp",
	blockDepth: number,
}

export type AstExprGlobal = {
	tag: "global",
	name: Token,
}

export type AstExprCall = {
	tag: "call",
	func: AstExpr, -- TODO: stricter?
	arguments: { AstExpr },
	openParens: Token<"(">?,
	commas: { Token<","> },
	closeParens: Token<")">?,
}

export type AstExprIndexName = {
	tag: "indexname",
	expr: AstExpr,
	index: string,
	indexLocation: Location,
	accessor: {
		position: Position,
		value: "." | ":",
	},
}

export type AstExprIndexExpr = {
	tag: "index",
	expr: AstExpr,
	index: AstExpr,
}

export type AstExprTableItem =
	| { kind: "list", value: AstExpr }
	| { kind: "record", key: string, equals: Token<"=">, value: AstExpr }
	| { kind: "general", key: string, equals: Token<"=">, value: AstExpr }

export type AstExprTable = {
	tag: "table",
	openBrace: Token<"{">,
	entries: { never },
	closeBrace: Token<"}">,
}

export type AstExpr =
	AstExprConstantString
	| AstExprGlobal
	| AstExprCall
	| AstExprIndexName
	| AstExprIndexExpr
	| AstExprTable

export type AstStatBlock = {
	tag: "block",
	statements: { AstStat },
}

export type AstStatReturn = {
	tag: "return",
	["return"]: Token<"return">,
	expressions: { AstExpr },
	commas: { Token<","> },
}

export type AstStatExpr = {
	tag: "expression",
	expression: AstExpr,
}

export type AstStatLocal = {
	tag: "local",
	["local"]: Token<"local">,
	variables: { AstLocal },
	variablesCommas: { Token<","> },
	equals: Token<"=">?,
	values: { AstExpr },
	valuesCommas: { Token<","> },
}

export type AstStat = AstStatBlock | AstStatReturn | AstStatExpr | AstStatLocal

return {}
