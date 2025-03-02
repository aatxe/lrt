--!strict
local fs = require("@lute/fs")

export type Context = {
	path: string,
	source: string,
}

export type Transform = (ctx: Context) -> string

local function apply(filePath: string, transform: Transform)
	local source = fs.readfiletostring(filePath)

	local ctx = {
		path = filePath,
		source = source,
	}

	local result = transform(ctx)

	-- fs.writestringtofile(filePath, result)
	-- print(`Applied to {filePath}`)

	print(result)
end

return {
	apply = apply,
}
