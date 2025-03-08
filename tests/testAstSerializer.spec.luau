local luau = require("@lute/luau")
local fs = require("@lute/fs")

local parser = require("@batteries/syntax/parser")
local printer = require("@batteries/syntax/printer")
local T = require("@batteries/syntax/ast_types")

local function assertEqualsLocation(
	actual: T.Location,
	startLine: number,
	startColumn: number,
	endLine: number,
	endColumn: number
)
	assert(actual.begin.line == startLine)
	assert(actual.begin.column == startColumn)
	assert(actual["end"].line == endLine)
	assert(actual["end"].column == endColumn)
end

local function test_tokenContainsLeadingSpaces()
	local block = luau.parse("  local x = 1").root
	assert(#block.statements == 1)

	local l = block.statements[1]
	assert(l.tag == "local")

	local token = l["local"]
	assert(#token.leadingTrivia == 1)
	assert(token.leadingTrivia[1].tag == "whitespace")
	assert(token.leadingTrivia[1].text == "  ")
	assertEqualsLocation(token.leadingTrivia[1].location, 0, 0, 0, 2)
end

local function test_tokenContainsLeadingNewline()
	local block = luau.parse("\n" .. "local x = 1").root
	assert(#block.statements == 1)

	local l = block.statements[1]
	assert(l.tag == "local")

	local token = l["local"]
	assert(#token.leadingTrivia == 1)
	assert(token.leadingTrivia[1].tag == "whitespace")
	assert(token.leadingTrivia[1].text == "\n")
	assertEqualsLocation(token.leadingTrivia[1].location, 0, 0, 1, 0)
end

local function test_tokenContainsLeadingSingleLineComment()
	local block = luau.parse("-- comment\n" .. "local x = 1").root
	assert(#block.statements == 1)

	local l = block.statements[1]
	assert(l.tag == "local")

	local token = l["local"]
	assert(#token.leadingTrivia == 2)
	assert(token.leadingTrivia[1].tag == "comment")
	assert(token.leadingTrivia[1].text == "-- comment")
	assertEqualsLocation(token.leadingTrivia[1].location, 0, 0, 0, 10)
	assert(token.leadingTrivia[2].tag == "whitespace")
	assert(token.leadingTrivia[2].text == "\n")
	assertEqualsLocation(token.leadingTrivia[2].location, 0, 10, 1, 0)
end

local function test_tokenContainsLeadingBlockComment()
	local block = luau.parse("--[[ comment ]] local x = 1").root
	assert(#block.statements == 1)

	local l = block.statements[1]
	assert(l.tag == "local")

	local token = l["local"]
	assert(#token.leadingTrivia == 2)
	assert(token.leadingTrivia[1].tag == "blockcomment")
	assert(token.leadingTrivia[1].text == "--[[ comment ]]")
	assertEqualsLocation(token.leadingTrivia[1].location, 0, 0, 0, 15)
	assert(token.leadingTrivia[2].tag == "whitespace")
	assert(token.leadingTrivia[2].text == " ")
	assertEqualsLocation(token.leadingTrivia[2].location, 0, 15, 0, 16)
end

local function test_tokenizeWhitespace()
	local block = luau.parse("  \n\t\t\n\n" .. "local x = 1").root
	assert(#block.statements == 1)

	local l = block.statements[1]
	assert(l.tag == "local")

	local token = l["local"]
	assert(#token.leadingTrivia == 3)
	assert(token.leadingTrivia[1].text == "  \n")
	assert(token.leadingTrivia[2].text == "\t\t\n")
	assert(token.leadingTrivia[3].text == "\n")
end

local function test_triviaSplitBetweenLeadingAndTrailing()
	local block = luau.parse("local x = 'test' -- comment\n" .. "-- comment 2\nlocal y = 'value'").root
	assert(#block.statements == 2)

	local firstStmt = block.statements[1]
	assert(firstStmt.tag == "local")

	local trailingToken = firstStmt.values[1].node
	assert(trailingToken.tag == "string")
	assert(#trailingToken.trailingTrivia == 3)
	assert(trailingToken.trailingTrivia[1].text == " ")
	assert(trailingToken.trailingTrivia[2].text == "-- comment")
	assert(trailingToken.trailingTrivia[3].text == "\n")

	local secondStmt = block.statements[2]
	assert(secondStmt.tag == "local")

	local leadingToken = secondStmt["local"]
	assert(#leadingToken.leadingTrivia == 2)
	assert(leadingToken.leadingTrivia[1].text == "-- comment 2")
	assert(leadingToken.leadingTrivia[2].text == "\n")
end

local function test_roundtrippableAst()
	local files = {
		"examples/a.luau",
		"examples/b.luau",
	}

	for _, path in files do
		local source = fs.readfiletostring(path)
		local result = printer.print(parser.parse(source))

		-- FIXME: we currently don't handle the EOF character (and its trivia) in the CST
		result ..= "\n"

		assert(source == result)
	end
end

test_tokenContainsLeadingSpaces()
test_tokenContainsLeadingNewline()
test_tokenContainsLeadingSingleLineComment()
test_tokenContainsLeadingBlockComment()
test_tokenizeWhitespace()
test_triviaSplitBetweenLeadingAndTrailing()
test_roundtrippableAst()
