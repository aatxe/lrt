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

export type AstExprGroup = {
	tag: "group",
	openParens: Token<"(">,
	expression: AstExpr,
	closeParens: Token<")">,
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

export type AstExprVarargs = Token<"..."> & { tag: "vararg" }

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
	openBrackets: Token<"[">,
	index: AstExpr,
	closeBrackets: Token<"]">,
}

export type AstFunctionBody = {
	-- TODO: generics
	openParens: Token<"(">,
	-- TODO: parameters
	parameters: Punctuated<Token<string>>,
	closeParens: Token<")">,
	-- TODO: return type
	body: AstStatBlock,
	["end"]: Token<"end">,
}

export type AstExprAnonymousFunction = {
	tag: "function",
	["function"]: Token<"function">,
	body: AstFunctionBody,
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

export type AstExprUnary = {
	tag: "unary",
	operator: Token<"not" | "-" | "#">,
	operand: AstExpr,
}

export type AstExprBinary = {
	tag: "binary",
	lhsoperand: AstExpr,
	operator: Token, -- TODO: enforce token type
	rhsoperand: AstExpr,
}

export type AstExpr =
	| AstExprGroup
	| AstExprConstantNil
	| AstExprConstantBool
	| AstExprConstantNumber
	| AstExprConstantString
	| AstExprLocal
	| AstExprGlobal
	| AstExprVarargs
	| AstExprCall
	| AstExprIndexName
	| AstExprIndexExpr
	| AstExprAnonymousFunction
	| AstExprTable
	| AstExprUnary
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

export type AstStatWhile = {
	tag: "while",
	["while"]: Token<"while">,
	condition: AstExpr,
	["do"]: Token<"do">,
	body: AstStatBlock,
	["end"]: Token<"end">,
}

export type AstStatRepeat = {
	tag: "repeat",
	["repeat"]: Token<"repeat">,
	body: AstStatBlock,
	["until"]: Token<"until">,
	condition: AstExpr,
}

export type AstStatBreak = Token<"break"> & { tag: "break" }

export type AstStatContinue = Token<"continue"> & { tag: "continue" }

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

export type AstStatFor = {
	tag: "for",
	["for"]: Token<"for">,
	variable: AstLocal,
	equals: Token<"=">,
	from: AstExpr,
	toComma: Token<",">,
	to: AstExpr,
	stepComma: Token<",">?,
	step: AstExpr?,
	["do"]: Token<"do">,
	body: AstStatBlock,
	["end"]: Token<"end">,
}

export type AstStatForIn = {
	tag: "forin",
	["for"]: Token<"for">,
	variables: Punctuated<Token<string>>,
	["in"]: Token<"in">,
	values: Punctuated<Token<string>>,
	["do"]: Token<"do">,
	body: AstStatBlock,
	["end"]: Token<"end">,
}

export type AstStatAssign = {
	tag: "assign",
	variables: Punctuated<AstExpr>,
	equals: Token<"=">,
	values: Punctuated<AstExpr>,
}

export type AstStatCompoundAssign = {
	tag: "compoundassign",
	variable: AstExpr,
	operand: Token, -- TODO: enforce token type,
	value: AstExpr,
}

export type AstStat =
	| AstStatBlock
	| AstStatIf
	| AstStatWhile
	| AstStatRepeat
	| AstStatBreak
	| AstStatContinue
	| AstStatReturn
	| AstStatExpr
	| AstStatLocal
	| AstStatFor
	| AstStatForIn
	| AstStatAssign
	| AstStatCompoundAssign

return {}
