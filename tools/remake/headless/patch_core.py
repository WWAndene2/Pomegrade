#!/usr/bin/env python3
"""The runner's copy of Azahar patched for the headless runs (Remake mod workflow, job headless), never the repository's:
run from the repository root after pomegrade_report.inc is appended to citra_libretro.cpp. Fails loudly if a line moved."""
# the file-open trace (fs_open_trace.inc) in place of OpenFile's debug line; fails loudly if Azahar's line moved
p = "azahar/src/core/hle/service/fs/fs_user.cpp"
s = open(p).read()
line = '    LOG_DEBUG(Service_FS, "path={}, mode={} attrs={}", file_path.DebugStr(), mode.hex, attributes);\n'
assert s.count(line) == 1, "OpenFile's debug line not found"
s = s.replace(line, open("tools/remake/headless/fs_open_trace.inc").read())
s = s.replace('#include "core/hle/kernel/process.h"', '#include "core/hle/kernel/process.h"\n#include "core/hle/kernel/thread.h"\n#include "core/memory.h"', 1)
open(p, "w").write(s)

# the GDB stub, which the libretro build switches off (azahar/CMakeLists.txt: ENABLE_GDBSTUB is "libretro incompatible"),
# compiled in anyway: the headless runs open it with "gdb PORT" (pomegrade_report.inc), for breakpoints on the game's code
p = "azahar/src/core/CMakeLists.txt"
s = open(p).read()
assert s.count("if (ENABLE_GDBSTUB)\n") == 1, "the GDB stub's block not found"
s = s.replace("if (ENABLE_GDBSTUB)\n", "if (ENABLE_GDBSTUB OR TRUE) # Pomegrade headless: the GDB stub always\n", 1)
open(p, "w").write(s)

# code coverage: the interpreter's dispatch reports each block it enters while a trace runs (pomegrade_report.inc,
# "trace on" / "trace off FILE"; the interpreter is chosen by POMEGRADE_INTERPRETER=1 in retro_host, the JIT bypasses this)
p = "azahar/src/core/arm/dyncom/arm_dyncom_interpreter.cpp"
s = open(p).read()
mark = "    // Find the cached instruction cream, otherwise translate it...\n"
assert s.count(mark) == 1, "the interpreter's dispatch not found"
s = s.replace(mark, "    if (pomegrade_trace_on) pomegrade_trace_block(cpu->Reg[15]); // Pomegrade headless: code coverage\n" + mark, 1)
inc = '#include "core/memory.h"\n'
assert s.count(inc) == 1, "the interpreter's includes moved"
s = s.replace(inc, inc + "extern bool pomegrade_trace_on;\nextern \"C\" void pomegrade_trace_block(u32 pc);\n", 1)
open(p, "w").write(s)
