-- The frontend witness for the NP2kai package: run the core inside Chimera
-- for a fixed number of frames with nothing pressed, then hash the whole
-- Main Memory domain. run-frontend.sh compares the hash with the native
-- reference (run-native's mem.bin after the same frames).
--
-- The job comes from the file named by the NP2_JOB environment variable:
--   frames=<how many frames to run>
--   meta=<where to write the result>
--   shot=<optional screenshot path>

local meta = { frames = 0, lag = 0, ramsize = 0, ramhash = "", probe = "" }
local function finish(status, detail)
	if meta.path then
		local f = assert(io.open(meta.path, "wb"))
		f:write(table.concat({
			"status=" .. status,
			"detail=" .. (detail or ""),
			"frames=" .. meta.frames,
			"lag=" .. meta.lag,
			"ramsize=" .. meta.ramsize,
			"ramhash=" .. meta.ramhash,
			"probe=" .. meta.probe,
		}, "\n") .. "\n")
		f:close()
	end
	client.exit()
end

local jobPath = os.getenv("NP2_JOB")
if jobPath == nil then error("NP2_JOB is not set") end
local job = {}
for line in io.lines(jobPath) do
	local k, v = line:match("^([^=]+)=(.*)$")
	if k then job[k] = v:gsub("\r$", "") end
end
meta.path = job.meta

if emu.getcorename() ~= "NP2kai" then
	finish("ERROR", "wrong core: " .. tostring(emu.getcorename()))
end
if emu.getsystemid() ~= "PC9821" then
	finish("ERROR", "wrong system id: " .. tostring(emu.getsystemid()))
end

pcall(function() client.speedmode(6400) end)
pcall(function() client.invisibleemulation(true) end)

-- to the frame count, not for a number of advances: the frontend may have
-- run a frame or two before the script took over
local frames = tonumber(job.frames) or 120
while emu.framecount() < frames do
	emu.frameadvance()
end

meta.frames = emu.framecount()
meta.lag = emu.lagcount()
local ok, err = pcall(function()
	memory.usememorydomain("Main Memory")
	meta.ramsize = memory.getcurrentmemorydomainsize()
	meta.ramhash = memory.hash_region(0, meta.ramsize, "Main Memory")
	-- a known address, read the way a script reads one: the test disk's
	-- boot sector (loaded at 1FC0:0000) keeps the BIOS's answer at offset
	-- 1Dh and its name after it
	local hex = {}
	for i = 0, 15 do
		hex[#hex + 1] = string.format("%02X", memory.readbyte(0x1FC1D + i, "Main Memory"))
	end
	meta.probe = table.concat(hex)
end)
if not ok then finish("ERROR", "could not read Main Memory: " .. tostring(err)) end

if job.shot ~= nil and job.shot ~= "" then
	client.screenshot(job.shot)
end
finish("OK", "")
