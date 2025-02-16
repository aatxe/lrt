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

export type AstExprConstantNil = Token<"nil"> & { tag: "nil" }

export type AstExprConstantBool = Token<"true" | "false"> & { tag: "boolean", value: boolean }

export type AstExprConstantNumber = Token<string> & { tag: "number", value: number }

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

export type AstExprTableItem = | { kind: "list", value: AstExpr, separator: Token<"," | ";">? } | {
	kind: "record",
	key: Token<string>,
	equals: Token<"=">,
	value: AstExpr,
	separator: Token<"," | ";">?,
} | {
	kind: "general",
	indexerOpen: Token<"[">,
	key: AstExpr,
	indexerClose: Token<"]">,
	equals: Token<"=">,
	value: AstExpr,
	separator: Token<"," | ";">?,
}

export type AstExprTable = {
	tag: "table",
	openBrace: Token<"{">,
	entries: { AstExprTableItem },
	closeBrace: Token<"}">,
}

export type AstExprBinary = {
	tag: "binary",
	lhsoperand: AstExpr,
	operator: Token, -- TODO: enforce token type
	rhsoperand: AstExpr,
}

export type AstExpr =
	| AstExprConstantNil
	| AstExprConstantBool
	| AstExprConstantNumber
	| AstExprConstantString
	| AstExprLocal
	| AstExprGlobal
	| AstExprCall
	| AstExprIndexName
	| AstExprIndexExpr
	| AstExprTable
	| AstExprBinary

export type AstStatBlock = {
	tag: "block",
	statements: { AstStat },
}

export type AstStatIf = {
	tag: "conditional",
	["if"]: Token<"if">,
	condition: AstExpr,
	["then"]: Token<"then">,
	consequent: AstStatBlock,
	["else"]: Token<"else">, -- TODO: this could be elseif!
	antecedent: AstStatBlock,
	["end"]: Token<"end">,
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

export type AstStat = AstStatBlock | AstStatIf | AstStatReturn | AstStatExpr | AstStatLocal

return {}
