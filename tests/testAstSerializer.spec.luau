local luau = require("@lute/luau")
local fs = require("@lute/fs")

local parser = require("@batteries/syntax/parser")
local printer = require("@batteries/syntax/printer")

local function test_tokenContainsLeadingSpaces()
	local block = luau.parse("  local x = 1").root
	assert(#block.statements == 1)

	local l = block.statements[1]
	assert(l.tag == "local")

	local token = l["local"]
	assert(#token.leadingTrivia == 1)
	assert(token.leadingTrivia[1].tag == "whitespace")
	assert(token.leadingTrivia[1].text == "  ")
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
	assert(token.leadingTrivia[2].tag == "whitespace")
	assert(token.leadingTrivia[2].text == "\n")
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
	assert(token.leadingTrivia[2].tag == "whitespace")
	assert(token.leadingTrivia[2].text == " ")
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
test_roundtrippableAst()
