local luau = {}

type ParseResult = {
	root: any,
	lines: number,
}

function luau.parse(source: string): ParseResult
	error("not implemented")
end

function luau.parseexpr(source: string): ParseResult
	error("not implemented")
end

return luau
