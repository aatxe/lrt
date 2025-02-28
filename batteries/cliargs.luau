local argparse = {}
argparse.__index = argparse

type ArgKind = "positional" | "flag" | "option"
type ArgOptions = {
	help: string?,
	aliases: { string }?,
	default: string?,
	required: boolean?,
}

type ArgData = {
	name: string,
	kind: ArgKind,
	options: ArgOptions,
}

type ParseResult = {
	values: { [string]: string },
	flags: { [string]: boolean },
	fwdArgs: { string },
}

type ParseData = {
	arguments: { [number]: ArgData },
	positional: { [number] : ArgData},
	parsed: { [number]: ParseResult },
}

type ParseInterface = typeof(argparse)
export type ParserType = setmetatable<ParseData, ParseInterface>

function argparse.new(): ParserType
	local self = setmetatable({}, argparse) :: ParserType
	self.arguments = {}
	self.positional = {}
	self.parsed = { values = {}, flags = {}, fwdArgs = {} }
	return self
end

function argparse.add(self: ParserType, name: string, kind: ArgKind, options: ArgOptions)
	local argument = {
		name = name,
		kind = kind,
		options = options or { aliases = {}, required = false },
	}
	table.insert(self.arguments, argument)

	if kind == "POSITIONAL" then
		table.insert(self.positional, argument)
	end
end

function argparse.parse(self: ParserType, args: {string}): ()
	local i = 1
	local pos_index = 1
	while i <= #args do
		local arg = args[i]
		
		if arg == "--" then
			for j = i + 1, #args do
				table.insert(self.parsed.fwdArgs, args[j])
			end
			break
		end
		
		if string.sub(arg, 1, 2) == "--" then
			local name = string.sub(arg, 3)
			local found = false
			for _, argument in self.arguments do
				local aliases = argument.options.aliases or {}
				if argument.name == name or table.find(aliases, name) then
					found = true
					if argument.kind == "OPTION" then
						i += 1
						assert(i <= #args, "Missing value for argument: " .. argument.name)
						self.parsed.values[argument.name] = args[i]
					else
						self.parsed.flags[argument.name] = true
					end
					break
				end
			end
			assert(found, "Unknown argument: " .. name)
		elseif string.sub(arg, 1, 1) == "-" then
			local flags = string.sub(arg, 2)
			for j = 1, #flags do
				local name = string.sub(flags, j, j)
				local found = false
				for _, argument in self.arguments do
					local aliases = argument.options.aliases or {}
					if argument.name == name or table.find(aliases, name) then
						found = true
						if argument.kind == "OPTION" then
							i += 1
							assert(i <= #args, "Missing value for argument: " .. argument.name)
							self.parsed.values[argument.name] = args[i]
						else
							self.parsed.flags[argument.name] = true
						end
						break
					end
				end
				assert(found, "Unknown argument: " .. name)
			end
		else
			if pos_index <= #self.positional then
				self.parsed.values[self.positional[pos_index].name] = arg
				pos_index += 1
			else
				table.insert(self.parsed.fwdArgs, arg)
			end
		end
		i += 1
	end

	for _, argument in self.arguments do
		assert(argument)
		if argument.options.required and self.parsed.values[argument.name] == nil then
			assert(self.parsed.values[argument.name], "Missing required argument: " .. argument.name)
		end
		if self.parsed.values[argument.name] == nil and argument.options.default then
			self.parsed.values[argument.name] = argument.options.default
		end
	end
end

function argparse.get(self: ParserType, name: string): boolean
	return self.parsed.values[name]
end

function argparse.has(self:ParserType, name: string): boolean
	return self.parsed.flags[name] ~= nil
end

function argparse.getFwdArgs(): { string } | nil
	return self.parsed.fwdArgs
end

function argparse.help(self: ParserType): ()
	print("Usage:")
	for _, argument in self.arguments do
		local aliasStr = table.concat(argument.options.aliases or {}, ", ")
		local reqStr = if argument.options.required then "(required) " else ""
		print(string.format("  --%s (-%s) - %s%s", argument.name, aliasStr, reqStr, argument.options.help or ""))
	end
end

return argparse
