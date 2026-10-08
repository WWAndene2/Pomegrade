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

# drawing skipped on demand and the software renderer's worker count set (retro_host's "draw off"; POMEGRADE_SW_THREADS):
# the renderer queues each triangle's scanlines to its workers and waits for them before the next triangle
# (RasterizerSoftware::ProcessTriangle, WaitForRequests), so a run spent most of its time synchronising (8 October: 60% of
# the 4 cores idle, the main thread at 58%, each worker at 25%, 21% system time)
p = "azahar/src/video_core/renderer_software/sw_rasterizer.cpp"
s = open(p).read()
ctor = "      num_sw_threads{std::max(std::thread::hardware_concurrency(), 2U)},\n"
assert s.count(ctor) == 1, "the renderer's worker count moved"
s = s.replace(ctor, "      num_sw_threads{PomegradeSwThreads()},\n", 1)
anon = "} // Anonymous namespace\n"
assert s.count(anon) == 1, "the renderer's anonymous namespace moved"
s = s.replace(anon, """std::atomic<bool> pomegrade_skip_draw{false};

std::size_t PomegradeSwThreads() {
    const char* given = std::getenv("POMEGRADE_SW_THREADS");
    return given ? std::max(std::atoi(given), 1) : std::max(std::thread::hardware_concurrency(), 2U);
}

} // Anonymous namespace

extern "C" __attribute__((visibility("default"))) void pomegrade_set_skip_draw(bool skip) {
    pomegrade_skip_draw.store(skip, std::memory_order_relaxed);
}
""", 1)
add = "                                     const Pica::OutputVertex& v2) {\n"
assert s.count(add) == 1, "AddTriangle's signature moved"
s = s.replace(add, add + "    if (pomegrade_skip_draw.load(std::memory_order_relaxed)) {\n        return;\n    }\n", 1)
s = s.replace("#include <boost/container/static_vector.hpp>", "#include <atomic>\n#include <cstdlib>\n#include <boost/container/static_vector.hpp>", 1)
open(p, "w").write(s)

# a state made by this very build is refused when the copy has no git information (8 October: the rebuilt core named its
# revision "UNKNOWN" and refused the title state it had just saved): the revision check only warns here, the headless runs
# making and loading their states with one build
p = "azahar/src/core/savestate.cpp"
s = open(p).read()
check = """                  Common::g_scm_rev, revision);
        return false;
    }"""
assert s.count(check) == 1, "the save state's revision check moved"
s = s.replace(check, """                  Common::g_scm_rev, revision);
        // Pomegrade headless: loaded anyway (patch_core.py)
    }""", 1)
open(p, "w").write(s)

# the light of each draw (pica_lights.inc; "lights on" / "lights off FILE" in retro_host, pomegrade_report.inc): recorded
# when a draw is triggered, before it runs
p = "azahar/src/video_core/pica/pica_core.cpp"
s = open(p).read()
inc = '#include "video_core/pica/vertex_loader.h"\n'
assert s.count(inc) == 1, "pica_core.cpp's includes moved"
s = s.replace(inc, inc + open("tools/remake/headless/pica_lights.inc").read(), 1)
draw = "        DrawArrays(is_indexed);\n"
assert s.count(draw) == 1, "the draw trigger moved"
s = s.replace(draw, "        if (pomegrade_lights_on) PomegradeRecordLights(regs.internal, vs_setup.uniforms); // Pomegrade headless\n" + draw, 1)
open(p, "w").write(s)

# the writers of a memory range ("writers LO HI" / "writers off FILE" in retro_host, pomegrade_report.inc): with the
# interpreter, each 32/64-bit write of the game into [LO, HI) is kept with the instruction's address and the return address
# (Reg[15], Reg[14]), to find the code that fills a structure (ORAS_ENGINE.md 2, the light)
p = "azahar/src/core/arm/skyeye_common/armstate.cpp"
s = open(p).read()
for size, call in (("32", "    memory.Write32(address, data);\n"), ("64", "    memory.Write64(address, data);\n")):
    assert s.count(call) == 1, "WriteMemory" + size + " moved"
    s = s.replace(call, "    if (address >= pomegrade_writers_lo && address < pomegrade_writers_hi) pomegrade_writer(address, (u32)data, Reg[15], Reg[14]); // Pomegrade headless\n" + call, 1)
s = s.replace('#include "core/memory.h"\n', '#include "core/memory.h"\nextern u32 pomegrade_writers_lo, pomegrade_writers_hi;\nextern "C" void pomegrade_writer(u32 address, u32 value, u32 pc, u32 lr);\n', 1)
assert "pomegrade_writers_lo, pomegrade_writers_hi;" in s, "armstate.cpp's includes moved"
open(p, "w").write(s)
