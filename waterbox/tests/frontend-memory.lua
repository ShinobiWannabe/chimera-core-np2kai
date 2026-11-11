-- The memory events witness for the NP2kai package (Chimera issues 496 and
-- 534; docs/memory-events.md), inside Chimera's window on the counter disk
-- (tests/gen-testdisk.py --counter). Its boot sector, at 1FC0:0000, adds one
-- to the word at 1FC70h once a frame, with the instruction at 1FC0:0016
-- (inc word [0070h]).
--
-- The job comes from the file named by the NP2_JOB environment variable:
--   meta=<where to write the result>
--   expect=on|off   whether the project has the setting Lua Memory Events
--   tag=<prefix>    the checks' names begin with it (mem21, mem2, memoff)
-- The result is one line a check, "<name>=PASS|FAIL|INFO <detail>", then
-- "status=OK" when the script got to the end.

local FRAMES, INC = 0x1FC70, 0x1FC16
local lines = {}
local meta, expect, tag = nil, "on", "mem"
local function out(name, ok, detail)
	local word = ok == nil and "INFO" or (ok and "PASS" or "FAIL")
	lines[#lines + 1] = tag .. ":" .. name .. "=" .. word .. " " .. (detail or "")
end
local function finish(status, detail)
	if meta then
		local f = assert(io.open(meta, "wb"))
		lines[#lines + 1] = "status=" .. status .. " " .. (detail or "")
		f:write(table.concat(lines, "\n") .. "\n")
		f:close()
	end
	client.exit()
end
local function check(name, fn)
	local ok, err = pcall(fn)
	if not ok then out(name, false, "error: " .. tostring(err)) end
end

local jobPath = os.getenv("NP2_JOB")
if jobPath == nil then error("NP2_JOB is not set") end
for line in io.lines(jobPath) do
	local k, v = line:match("^([^=]+)=(.*)$")
	if k then v = v:gsub("\r$", "") end
	if k == "meta" then meta = v elseif k == "expect" then expect = v elseif k == "tag" then tag = v end
end

pcall(function() client.speedmode(6400) end)
pcall(function() client.invisibleemulation(true) end)

local function u16(a) return memory.read_u16_le(a, "System Bus") end
local function advance(n) for _ = 1, n do emu.frameadvance() end end

while u16(FRAMES) < 20 and emu.framecount() < 3000 do emu.frameadvance() end
if u16(FRAMES) < 20 then finish("ERROR", "the counter disk never started counting") return end

if expect == "off" then
	-- the setting off: no scopes, and an event is refused with Chimera's words
	check("refused", function()
		local scopes = event.availableScopes()
		local ok, err = pcall(event.on_bus_write, function() end, FRAMES, "np2-off")
		pcall(function() event.unregisterbyname("np2-off") end)
		out("refused", scopes[0] == nil and not ok,
			string.format("%s; on_bus_write %s", scopes[0] == nil and "no scopes" or ("scope " .. tostring(scopes[0])),
				ok and "was taken" or ("refused: " .. tostring(err))))
	end)
	finish("OK", "")
	return	-- client.exit() does not stop the script part way
end

check("scopes", function()
	local scopes = event.availableScopes()
	local r, w, x = event.can_replace_value("read"), event.can_replace_value("write"), event.can_replace_value("exec")
	-- Chimera's list counts from 0, as BizHawk's
	out("scopes", scopes[0] == "System Bus" and scopes[1] == nil and r and w and not x,
		string.format("scopes {%s, %s}; replace read %s, write %s, exec %s", tostring(scopes[0]), tostring(scopes[1]),
			tostring(r), tostring(w), tostring(x)))
end)

-- (the same measure, before any other watch was made: for comparison)
firstAny = "not made"
check("exec-any-first", function()
	local n, id = 0, nil
	id = event.on_bus_exec_any(function() n = n + 1 end, "np2-any0")
	local c = {}
	for i = 1, 3 do n = 0; advance(1); c[i] = n end
	event.unregisterbyid(id)
	firstAny = string.format("%d, %d, %d", c[1], c[2], c[3])
end)

-- a write of the counter: once a frame, a word, each value one more
check("write", function()
	local vals, bad = {}, nil
	local id = event.on_bus_write(function(addr, val, flags)
		vals[#vals + 1] = val
		if addr ~= FRAMES or (flags & 0x2000) == 0 or (flags & 0x30000) ~= 0x20000 then
			bad = string.format("addr %X flags %X", addr, flags)
		end
	end, FRAMES, "np2-write")
	advance(10)
	event.unregisterbyid(id)
	local step = #vals > 1
	for i = 2, #vals do if vals[i] ~= vals[i - 1] + 1 then step = false end end
	out("write", #vals == 10 and step and bad == nil,
		string.format("%d calls in 10 frames, values %s..%s%s", #vals, tostring(vals[1]), tostring(vals[#vals]),
			bad and (", " .. bad) or ""))
end)

-- read twice a frame: by the inc, then by the mov that shows it on screen,
-- which reads the new value
check("read", function()
	local n, last = 0, nil
	local id = event.on_bus_read(function(addr, val) n = n + 1; last = val end, FRAMES, "np2-read")
	advance(10)
	event.unregisterbyid(id)
	out("read", n == 20 and last == u16(FRAMES), string.format("%d calls in 10 frames, last value %s, word now %d", n, tostring(last), u16(FRAMES)))
end)

-- the instruction, before it runs: once a frame, with the registers as
-- they are then (CS 1FC0h, IP 0016h)
check("exec", function()
	local n, regs = 0, "not read"
	local id = event.on_bus_exec(function(addr)
		n = n + 1
		local ok, r = pcall(emu.getregisters)
		if ok and type(r) == "table" then
			local ip = r["IP"] or r["EIP"]
			regs = string.format("CS %s IP %s", r["CS"] and string.format("%X", r["CS"]) or "?",
				ip and string.format("%X", ip) or "?")
		elseif not ok then regs = "getregisters: " .. tostring(r) end
	end, INC, "np2-exec")
	advance(10)
	event.unregisterbyid(id)
	out("exec", n == 10, string.format("%d calls in 10 frames", n))
	out("exec-registers", regs == "CS 1FC0 IP 16", "inside the call: " .. regs)
end)

-- a function giving back a number: the machine writes that instead
check("replace", function()
	local id = event.on_bus_write(function() return 0x1234 end, FRAMES, "np2-replace")
	advance(3)
	local held = u16(FRAMES)
	event.unregisterbyid(id)
	advance(2)
	local after = u16(FRAMES)
	out("replace", held == 0x1234 and after == 0x1236,
		string.format("word %X while replaced, %X two frames after", held, after))
end)

-- every instruction of a frame, three frames, each counted (at least the
-- wait loop's and the inc's), and where the first frame's ran
local function anyCounts()
	local counts, n, at = {}, 0, {}
	local id = event.on_bus_exec_any(function(addr)
		n = n + 1
		if #counts == 0 then at[addr] = (at[addr] or 0) + 1 end
	end, "np2-any")
	for i = 1, 3 do n = 0; advance(1); counts[i] = n end
	event.unregisterbyid(id)
	return counts, at
end
check("exec-any", function()
	local c, at = anyCounts()
	-- not a fixed number: the counter's wait loop reads the GDC status port,
	-- dear in CPU time, so the 286 machine at x4 runs about a hundred
	-- instructions a frame there and the IA-32 one at x20 tens of thousands
	out("exec-any", c[1] >= 10 and c[2] >= 10 and c[3] >= 10 and at[INC] == 1,
		string.format("%d, %d, %d instructions in three frames, the inc %s times in the first (first measure %s)",
			c[1], c[2], c[3], tostring(at[INC] or 0), firstAny))
	local list = {}
	for addr, k in pairs(at) do list[#list + 1] = {addr, k} end
	table.sort(list, function(x, y) return x[2] > y[2] or (x[2] == y[2] and x[1] < y[1]) end)
	local parts = {}
	for i = 1, math.min(8, #list) do parts[#parts + 1] = string.format("%X*%d", list[i][1], list[i][2]) end
	out("exec-any-where", nil, string.format("%d addresses, most run: %s", #list, table.concat(parts, " ")))
end)

-- (every read and write of a frame: 25 and 31 on the IA-32 machine, about
-- 250 and 31 on the 286, as run-native says)
check("rw-any", function()
	local r, w = 0, 0
	local a = event.on_bus_read(function() r = r + 1 end, nil, "np2-rany")
	local b = event.on_bus_write(function() w = w + 1 end, nil, "np2-wany")
	advance(1)
	event.unregisterbyid(a); event.unregisterbyid(b)
	out("rw-any", nil, string.format("%d reads, %d writes in one frame", r, w))
end)

finish("OK", "")
