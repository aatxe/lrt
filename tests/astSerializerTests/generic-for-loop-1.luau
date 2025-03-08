for index, value in pairs(list) do
	call(index, value)
end

for index, value in next, list do
	call(index, value)
end

for index, value in list do
	call(index, value)
end
