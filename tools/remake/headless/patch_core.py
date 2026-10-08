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
