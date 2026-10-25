-- The tools witness for the NP2kai package: what RAM Search, RAM Watch and
-- Lua scripts stand on, checked inside Chimera on the counter disk
-- (tests/gen-testdisk.py --counter). Its boot sector, at 1FC0:0000, counts
-- the frames in a word at 1FC70h and the keys it reads in a byte at 1FC72h,
-- the last key's code and character at 1FC74h, and shows both counts in hex
-- at the top left of the text screen.
--
-- The job comes from the file named by the NP2_JOB environment variable:
--   meta=<where to write the result>
-- The result is one line a check, "<name>=PASS|FAIL|INFO <detail>", then
-- "status=OK" when the script got to the end.

local FRAMES, KEYS, LASTKEY = 0x1FC70, 0x1FC72, 0x1FC74
local lines = {}
local meta
local function out(name, ok, detail)
	local word = ok == nil and "INFO" or (ok and "PASS" or "FAIL")
	lines[#lines + 1] = name .. "=" .. word .. " " .. (detail or "")
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
	if k == "meta" then meta = v:gsub("\r$", "") end
end

pcall(function() client.speedmode(6400) end)
pcall(function() client.invisibleemulation(true) end)

local function u16(a) return memory.read_u16_le(a, "Main Memory") end
local function u8(a) return memory.readbyte(a, "Main Memory") end
-- the text screen's first cells as characters: the counts as the machine shows them
local function screen(n)
	local s = {}
	for i = 0, n - 1 do s[#s + 1] = string.char(memory.readbyte(i * 2, "Text VRAM")) end
	return table.concat(s)
end
local function advance(n) for _ = 1, n do emu.frameadvance() end end

-- the counter disk starts counting once the machine has booted it
while u16(FRAMES) < 20 and emu.framecount() < 3000 do emu.frameadvance() end
if u16(FRAMES) < 20 then finish("ERROR", "the counter disk never started counting") end

-- every domain the core declares, with its size
check("domains", function()
	local want = {
		["Main Memory"] = 0x200000, ["Extended Memory"] = 13 * 1024 * 1024, ["Text VRAM"] = 0x4000,
		["Graphics VRAM 0"] = 0x18000, ["Graphics VRAM 0 E"] = 0x8000,
		["Graphics VRAM 1"] = 0x18000, ["Graphics VRAM 1 E"] = 0x8000, ["Font"] = 0x84000,
	}
	local got, bad = {}, {}
	for _, name in pairs(memory.getmemorydomainlist()) do got[name] = memory.getmemorydomainsize(name) end
	for name, size in pairs(want) do
		if got[name] ~= size then bad[#bad + 1] = name .. " " .. tostring(got[name]) end
	end
	out("domains", #bad == 0, #bad == 0 and "8 domains, each its size" or table.concat(bad, ", "))
end)

-- the named domains are views of Main Memory: the same bytes both ways
check("views", function()
	local pairs_ = {
		{ "Text VRAM", 0xA0000, 0x4000 }, { "Graphics VRAM 0", 0xA8000, 0x18000 },
		{ "Font", 0x110000, 0x84000 },
	}
	local bad = {}
	for _, p in ipairs(pairs_) do
		if memory.hash_region(0, p[3], p[1]) ~= memory.hash_region(p[2], p[3], "Main Memory") then
			bad[#bad + 1] = p[1]
		end
	end
	-- a write through the view reads back through Main Memory, and back
	local old = memory.readbyte(0x3F00, "Text VRAM")
	memory.writebyte(0x3F00, 0x5A, "Text VRAM")
	local seen = u8(0xA3F00)
	memory.writebyte(0xA3F00, old, "Main Memory")
	local back = memory.readbyte(0x3F00, "Text VRAM")
	local ok = #bad == 0 and seen == 0x5A and back == old
	out("views", ok, ok and "Text VRAM, Graphics VRAM 0 and Font are Main Memory's bytes; a write shows in both"
		or ("differ: " .. table.concat(bad, ", ") .. string.format(" write %02X back %02X", seen, back)))
end)

-- the value changes once a frame, as a search over frames needs
check("ticks", function()
	local a = u16(FRAMES)
	local steps = {}
	for _ = 1, 10 do emu.frameadvance(); local b = u16(FRAMES); steps[#steps + 1] = (b - a) % 0x10000; a = b end
	local all1 = true
	for _, d in ipairs(steps) do if d ~= 1 then all1 = false end end
	out("ticks", all1, "the frame word grew by " .. table.concat(steps, ",") .. " over 10 frames")
end)

-- a script's search over the domain finds the frame word among every
-- address, the way RAM Search narrows it: keep the words that went up by
-- one at each of three frames
check("search", function()
	local size = 0x20000	-- the first 128 KB: the boot sector is at 1FC00h
	local function snap() return memory.read_bytes_as_array(0, size, "Main Memory") end
	local before = snap()
	local cand = nil
	for _ = 1, 3 do
		emu.frameadvance()
		local now = snap()
		local keep = {}
		if cand == nil then
			for a = 1, size - 1, 2 do
				local p = before[a] + before[a + 1] * 256
				local n = now[a] + now[a + 1] * 256
				if (n - p) % 0x10000 == 1 then keep[#keep + 1] = a - 1 end
			end
		else
			for _, a in ipairs(cand) do
				local p = before[a + 1] + before[a + 2] * 256
				local n = now[a + 1] + now[a + 2] * 256
				if (n - p) % 0x10000 == 1 then keep[#keep + 1] = a end
			end
		end
		cand, before = keep, now
	end
	local found = false
	for _, a in ipairs(cand) do if a == FRAMES then found = true end end
	local shown = {}
	for i = 1, math.min(#cand, 4) do shown[#shown + 1] = string.format("%X", cand[i]) end
	out("search", found, #cand .. " address(es) left: " .. table.concat(shown, " "))
end)

-- a poke reaches the machine: the program counts on from the value written
-- and shows it on the screen
check("poke", function()
	memory.write_u16_le(FRAMES, 0x4000, "Main Memory")
	advance(3)
	local v = u16(FRAMES)
	advance(1)
	local s = screen(4)
	local ok = v == 0x4003 and s:sub(1, 3) == "400"
	out("poke", ok, string.format("wrote 4000h, read %04Xh three frames later; the screen shows %s", v, s))
end)

-- Extended Memory takes a write and gives it back, at both ends
check("extended", function()
	local top = 13 * 1024 * 1024 - 4
	local o0, o1 = memory.read_u32_le(0, "Extended Memory"), memory.read_u32_le(top, "Extended Memory")
	memory.write_u32_le(0, 0x12345678, "Extended Memory")
	memory.write_u32_le(top, 0x9ABCDEF0, "Extended Memory")
	local r0, r1 = memory.read_u32_le(0, "Extended Memory"), memory.read_u32_le(top, "Extended Memory")
	memory.write_u32_le(0, o0, "Extended Memory"); memory.write_u32_le(top, o1, "Extended Memory")
	out("extended", r0 == 0x12345678 and r1 == 0x9ABCDEF0, string.format("read back %08X and %08X", r0, r1))
end)

-- the other reads and writes a script uses agree with each other
check("widths", function()
	local a = 0x1FD00	-- past the program, inside the boot sector's 1 KB
	memory.write_u32_le(a, 0x11223344, "Main Memory")
	local ok = memory.read_u16_le(a, "Main Memory") == 0x3344 and memory.read_u16_be(a, "Main Memory") == 0x4433
		and memory.read_u24_le(a, "Main Memory") == 0x223344 and memory.read_u32_be(a, "Main Memory") == 0x44332211
		and mainmemory.read_u32_le(a) == 0x11223344 and mainmemory.readbyte(a + 3) == 0x11
	local r = memory.readbyterange(a, 4, "Main Memory")
	ok = ok and r[0] == 0x44 and r[3] == 0x11
	memory.write_u32_le(a, 0, "Main Memory")
	out("widths", ok, "8, 16, 24 and 32 bits, both byte orders, mainmemory and readbyterange")
end)

-- the registers: the CPU is in the counter's loop
check("registers", function()
	local r = emu.getregisters()
	local cs, ip = r["CS"], r["EIP"] or r["IP"]
	out("registers", cs == 0x1FC0 and ip ~= nil and ip < 0x70,
		string.format("CS=%X IP=%X (the counter's code is 1FC0:0000-006F)", cs or -1, ip or -1))
end)

-- a key pressed by the script is read by the program
check("joypad", function()
	local name
	for k, _ in pairs(joypad.get()) do if k:match("Key RETURN$") then name = k end end
	if name == nil then out("joypad", false, "no Key RETURN button"); return end
	local k0 = u8(KEYS)
	joypad.set({ [name] = true }); emu.frameadvance()
	joypad.set({ [name] = true }); emu.frameadvance()
	advance(10)
	local k1, code = u8(KEYS), u8(LASTKEY + 1)
	out("joypad", k1 == (k0 + 1) % 256 and code == 0x1C,
		string.format("'%s' held 2 frames: key count %d -> %d, key code %02Xh", name, k0, k1, code))
end)

-- a state kept in memory takes the machine back
check("state", function()
	local f0, v0 = emu.framecount(), u16(FRAMES)
	local id = memorysavestate.savecorestate()
	advance(30)
	local v1 = u16(FRAMES)
	memorysavestate.loadcorestate(id)
	local f2, v2 = emu.framecount(), u16(FRAMES)
	memorysavestate.removestate(id)
	out("state", v1 ~= v0 and v2 == v0,
		string.format("frame %d word %04X, 30 frames on %04X, loaded back: frame %d word %04X", f0, v0, v1, f2, v2))
end)

-- the frame events run once a frame
check("events", function()
	local starts, ends = 0, 0
	local a = event.onframestart(function() starts = starts + 1 end, "np2-start")
	local b = event.onframeend(function() ends = ends + 1 end, "np2-end")
	advance(20)
	event.unregisterbyid(a); event.unregisterbyid(b)
	out("events", starts == 20 and ends == 20, string.format("over 20 frames: %d starts, %d ends", starts, ends))
end)

-- memory callbacks (onmemorywrite and the rest): said, not required
check("callbacks", function()
	local hits = 0
	local ok, err = pcall(function()
		event.onmemorywrite(function() hits = hits + 1 end, FRAMES, "np2-write", "Main Memory")
	end)
	advance(5)
	pcall(function() event.unregisterbyname("np2-write") end)
	if ok and hits > 0 then out("callbacks", nil, "onmemorywrite ran " .. hits .. " times in 5 frames")
	else out("callbacks", nil, "onmemorywrite: " .. (ok and "registered, never ran" or tostring(err))) end
end)

-- drawing on the screen from a script does not stop it
check("gui", function()
	gui.text(10, 10, "np2kai tools check")
	gui.addmessage("np2kai tools check")
	emu.frameadvance()
	out("gui", true, "gui.text and gui.addmessage")
end)

finish("OK", "")
