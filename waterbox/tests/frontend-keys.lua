-- The bindings the machine has in Chimera's window, for run-frontend.sh to
-- hold to the package's default_keybinds.json (check-keybinds.py). The
-- window keeps only the keys a person changed, and the gate's settings start
-- empty, so with nothing changed these are the package's.
--
-- The job comes from the file named by the NP2_JOB environment variable:
--   out=<the bindings, JSON>   meta=<status=OK when written>
local jobPath = os.getenv("NP2_JOB")
if jobPath == nil then error("NP2_JOB is not set") end
local job = {}
for line in io.lines(jobPath) do
	local k, v = line:match("^([^=]+)=(.*)$")
	if k then job[k] = v:gsub("\r$", "") end
end
local f = assert(io.open(job.out, "wb"))
f:write(chimera.controls.effective())
f:close()
local m = assert(io.open(job.meta, "wb"))
m:write("status=OK\n")
m:close()
client.exit()
