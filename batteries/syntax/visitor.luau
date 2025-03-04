--!strict

local T = require("./ast_types")

type Visitor = {
	visitBlock: (T.AstStatBlock) -> boolean,
	visitReturn: (T.AstStatReturn) -> boolean,
	visitLocalDeclaration: (T.AstStatLocal) -> boolean,

	visitLocalReference: (T.AstExprLocal) -> boolean,
	visitGlobal: (T.AstExprGlobal) -> boolean,
	visitCall: (T.AstExprCall) -> boolean,
	visitBinary: (T.AstExprBinary) -> boolean,
	visitTable: (T.AstExprTable) -> boolean,
	visitIndexName: (T.AstExprIndexName) -> boolean,

	visitToken: (T.Token) -> boolean,
	visitString: (T.AstExprConstantString) -> boolean,
	visitLocal: (T.AstLocal) -> boolean,
}

local function alwaysVisit(...: any)
	return true
end

local defaultVisitor: Visitor = {
	visitBlock = alwaysVisit :: any,
	visitReturn = alwaysVisit :: any,
	visitLocalDeclaration = alwaysVisit :: any,

	visitLocalReference = alwaysVisit :: any,
	visitGlobal = alwaysVisit :: any,
	visitCall = alwaysVisit :: any,
	visitBinary = alwaysVisit :: any,
	visitTable = alwaysVisit :: any,
	visitIndexName = alwaysVisit :: any,

	visitToken = alwaysVisit :: any,
	visitString = alwaysVisit :: any,
	visitLocal = alwaysVisit :: any,
}

local function exhaustiveMatch(value: never): never
	error(`Unknown value in exhaustive match: {value}`)
end

local function visitToken<T>(token: T.Token<T>, visitor: Visitor)
	visitor.visitToken(token)
end

local function visitPunctuated<T, Separator>(list: T.Punctuated<T, Separator>, visitor: Visitor, apply: (T, Visitor) -> ())
	for _, item in list do
		apply(item.node, visitor)
		if item.separator then
			visitToken(item.separator, visitor)
		end
	end
end

local function visitLocal(node: T.AstLocal, visitor: Visitor)
	if visitor.visitLocal(node) then
		visitToken(node.name, visitor)
	end
end

local function visitBlock(block: T.AstStatBlock, visitor: Visitor)
	if visitor.visitBlock(block) then
		for _, statement in block.statements do
			visitStatement(statement, visitor)
		end
	end
end

local function visitReturn(node: T.AstStatReturn, visitor: Visitor)
	if visitor.visitReturn(node) then
		visitToken(node["return"], visitor)
		visitPunctuated(node.expressions, visitor, visitExpression)
	end
end

local function visitLocalStatement(node: T.AstStatLocal, visitor: Visitor)
	if visitor.visitLocalDeclaration(node) then
		visitToken(node["local"], visitor)
		visitPunctuated(node.variables, visitor, visitLocal)
		if node.equals then
			visitToken(node.equals, visitor)
		end
		visitPunctuated(node.values, visitor, visitExpression)
	end
end

local function visitString(node: T.AstExprConstantString, visitor: Visitor)
	if visitor.visitString(node) then
		visitor.visitToken(node)
	end
end

local function visitLocalReference(node: T.AstExprLocal, visitor: Visitor)
	if visitor.visitLocalReference(node) then
		visitor.visitToken(node.token)
	end
end

local function visitGlobal(node: T.AstExprGlobal, visitor: Visitor)
	if visitor.visitGlobal(node) then
		visitor.visitToken(node.name)
	end
end

local function visitCall(node: T.AstExprCall, visitor: Visitor)
	if visitor.visitCall(node) then
		visitExpression(node.func, visitor)
		if node.openParens then
			visitToken(node.openParens, visitor)
		end
		visitPunctuated(node.arguments, visitor, visitExpression)
		if node.closeParens then
			visitToken(node.closeParens, visitor)
		end
	end
end

local function visitBinary(node: T.AstExprBinary, visitor: Visitor)
	if visitor.visitBinary(node) then
		visitExpression(node.lhsoperand, visitor)
		visitToken(node.operator, visitor)
		visitExpression(node.rhsoperand, visitor)
	end
end

local function visitTable(node: T.AstExprTable, visitor: Visitor)
	if visitor.visitTable(node) then
		visitToken(node.openBrace, visitor)
		-- TODO: visit entries
		visitToken(node.closeBrace, visitor)
	end
end

local function visitIndexName(node: T.AstExprIndexName, visitor: Visitor)
	if visitor.visitIndexName(node) then
		visitExpression(node.expr, visitor)
		visitToken(node.accessor, visitor)
		visitToken(node.index, visitor)
	end
end

function visitExpression(expression: T.AstExpr, visitor: Visitor)
	if expression.tag == "string" then
		visitString(expression, visitor)
	elseif expression.tag == "local" then
		visitLocalReference(expression, visitor)
	elseif expression.tag == "global" then
		visitGlobal(expression, visitor)
	elseif expression.tag == "call" then
		visitCall(expression, visitor)
	elseif expression.tag == "binary" then
		visitBinary(expression, visitor)
	elseif expression.tag == "table" then
		visitTable(expression, visitor)
	elseif expression.tag == "indexname" then
		visitIndexName(expression, visitor)
	else
		exhaustiveMatch(expression.tag)
	end
end

function visitStatement(statement: T.AstStat, visitor: Visitor)
	if statement.tag == "block" then
		visitBlock(statement, visitor)
	elseif statement.tag == "expression" then
		visitExpression(statement.expression, visitor)
	elseif statement.tag == "local" then
		visitLocalStatement(statement, visitor)
	elseif statement.tag == "return" then
		visitReturn(statement, visitor)
	else
		exhaustiveMatch(statement.tag)
	end
end

local function createVisitor()
	return table.clone(defaultVisitor)
end

return {
	createVisitor = createVisitor,
	visitBlock = visitBlock,
}
