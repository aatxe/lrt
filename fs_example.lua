tempfile = fs.openfile("temp", "r")
tempstr = fs.readtostring(tempfile)
print(tempstr)

-- now writing
destfile = fs.openfile("dest", "w")

fs.writestringtofile(destfile, tempstr)
fs.close(destfile)

-- comparing
--df = fs.openfile("dest", "r")
--deststr = fs.readtostring(df)

--print(tempstr == deststr)
