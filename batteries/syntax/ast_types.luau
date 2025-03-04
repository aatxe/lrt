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

export type SingleLineComment = {
	tag: "comment",
	location: Location,
	text: string,
}

export type MultiLineComment = {
	tag: "blockcomment",
	location: Location,
	text: string,
	-- TODO: depth: number,
}

export type Trivia = Whitespace | SingleLineComment | MultiLineComment

export type Token<Kind = string> = {
	read leadingTrivia: { Trivia },
	read position: Position,
	read text: Kind,
	read trailingTrivia: { Trivia },
}

export type Pair<T, Separator> = { node: T, separator: Token<Separator>? }
export type Punctuated<T, Separator = ","> = { Pair<T, Separator> }

export type AstLocal = {
	name: Token<string>,
}

export type AstExprConstantString = Token<string> & {
	tag: "string",
	quoteStyle: "single" | "double" | "block" | "interp",
	blockDepth: number,
}

export type AstExprLocal = {
	tag: "local",
	token: Token<string>,
	["local"]: AstLocal,
	upvalue: boolean,
}

export type AstExprGlobal = {
	tag: "global",
	name: Token,
}

export type AstExprCall = {
	tag: "call",
	func: AstExpr, -- TODO: stricter?
	openParens: Token<"(">?,
	arguments: Punctuated<AstExpr>,
	closeParens: Token<")">?,
}

export type AstExprIndexName = {
	tag: "indexname",
	expr: AstExpr,
	accessor: Token<"." | ":">,
	index: Token<string>,
	indexLocation: Location,
}

export type AstExprIndexExpr = {
	tag: "index",
	expr: AstExpr,
	index: AstExpr,
}

export type AstExprBinary = {
	tag: "binary",
	lhsoperand: AstExpr,
	operator: Token, -- TODO: enforce token type
	rhsoperand: AstExpr,
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
	| AstExprConstantString
	| AstExprLocal
	| AstExprGlobal
	| AstExprCall
	| AstExprIndexName
	| AstExprIndexExpr
	| AstExprBinary
	| AstExprTable

export type AstStatBlock = {
	tag: "block",
	statements: { AstStat },
}

export type AstStatReturn = {
	tag: "return",
	["return"]: Token<"return">,
	expressions: Punctuated<AstExpr>,
}

export type AstStatExpr = {
	tag: "expression",
	expression: AstExpr,
}

export type AstStatLocal = {
	tag: "local",
	["local"]: Token<"local">,
	variables: Punctuated<AstLocal>,
	equals: Token<"=">?,
	values: Punctuated<AstExpr>,
}

export type AstStat = AstStatBlock | AstStatReturn | AstStatExpr | AstStatLocal

return {}
