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
# the software renderer crashes reading a texel (every headless run with the save, at the title screen's exit: SIGSEGV in
# LookupTexelInTile from TextureColor)
p = "azahar/src/video_core/renderer_software/sw_rasterizer.cpp"
s = open(p).read()
line = "            texture_color[i] = LookupTexture(texture_data, s, t, info);\n"
assert s.count(line) == 1, "the software renderer's texture lookup not found"
# runs 56, 60 and 63 crashed there even with the pointer, the texture's last byte and the coordinates checked: no texel is read
# at all (black). The headless runs need the field's modules and the threads, not the picture
s = s.replace(line, "            (void)texture_data; (void)info;\n            texture_color[i] = Common::Vec4<u8>{0, 0, 0, 255};\n")
open(p, "w").write(s)
