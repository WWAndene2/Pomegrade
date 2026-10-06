#include "Layout.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace remake
{

static float Fl(const Bytes& b, size_t at) { const uint32_t u = U32(b, at); float f; std::memcpy(&f, &u, 4); return f; }

// a name up to its zero byte, at most 64 characters, and never past the file's end
static std::string Name(const Bytes& b, size_t at, size_t most = 64)
{
    if (at >= b.size()) throw FormatError("layout: a name past the end of the file");
    return Text(b, at, std::min(most, b.size() - at));
}

static std::string Names(const Bytes& b, int count, size_t tableAt, size_t base)
{
    std::string s;
    for (int i = 0; i < count; i++) s += " " + Name(b, base + U32(b, tableAt + 4 * i));
    return s;
}

// printf into a string of any length
template <typename... A> static std::string Fmt(const char* format, A... args)
{
    const int n = snprintf(nullptr, 0, format, args...);
    std::string s((size_t)std::max(n, 0) + 1, '\0');
    snprintf(&s[0], s.size(), format, args...);
    s.resize((size_t)std::max(n, 0));
    return s;
}

std::string DescribeLayout(const Bytes& f)
{
    const std::string magic = Text(f, 0, 4);
    if ((magic != "CLYT" && magic != "CLAN") || U16(f, 4) != 0xFEFF) throw FormatError("layout: no CLYT/CLAN header");
    std::string out;
    std::string line;
    line = Fmt("%s version 0x%08X, %u bytes, %u sections\n", magic.c_str(), U32(f, 8), U32(f, 12), U16(f, 16));
    out += line;
    size_t at = U16(f, 6);
    int depth = 0;
    for (int s = 0; s < U16(f, 16) && at + 8 <= f.size(); s++)
    {
        const std::string kind = Text(f, at, 4);
        const uint32_t size = U32(f, at + 4);
        if (size < 8) throw FormatError("layout: section " + kind + " of size " + std::to_string(size));
        const std::string indent(2 * depth + 2, ' ');
        if (kind == "lyt1")
            line = Fmt("%slyt1: screen %g x %g, origin %s\n", indent.c_str(), Fl(f, at + 12), Fl(f, at + 16), f[at + 8] ? "centre" : "corner");
        else if (kind == "txl1" || kind == "fnl1")
        {
            const int count = U16(f, at + 8);
            line = Fmt("%s%s: %d%s\n", indent.c_str(), kind.c_str(), count, Names(f, count, at + 12, at + 12).c_str());
        }
        else if (kind == "mat1")
        {
            const int count = U16(f, at + 8);
            std::string names;
            for (int i = 0; i < count; i++) names += " " + Name(f, at + U32(f, at + 12 + 4 * i), 20);
            line = Fmt("%smat1: %d materials:%s\n", indent.c_str(), count, names.c_str());
        }
        else if (kind == "pan1" || kind == "pic1" || kind == "txt1" || kind == "wnd1" || kind == "bnd1")
        {
            // the pane block: flags, origin, alpha, scale flag, name (16), user data (8), translation, rotation, scale, size
            line = Fmt("%s%s %s: at (%g, %g, %g), rotation (%g, %g, %g), scale (%g, %g), size %g x %g, alpha %u%s\n",
                     indent.c_str(), kind.c_str(), Name(f, at + 12, 16).c_str(), Fl(f, at + 0x24), Fl(f, at + 0x28), Fl(f, at + 0x2C),
                     Fl(f, at + 0x30), Fl(f, at + 0x34), Fl(f, at + 0x38), Fl(f, at + 0x3C), Fl(f, at + 0x40), Fl(f, at + 0x44), Fl(f, at + 0x48),
                     f[at + 10], (f[at + 8] & 1) ? "" : ", hidden");
        }
        else if (kind == "grp1")
            line = Fmt("%sgrp1 %s: %u panes\n", indent.c_str(), Name(f, at + 8, 16).c_str(), U16(f, at + 24));
        else if (kind == "pat1")
            line = Fmt("%spat1: animation %s, frames %d to %d, %u groups\n", indent.c_str(), Name(f, at + U32(f, at + 12)).c_str(),
                     (int16_t)U16(f, at + 20), (int16_t)U16(f, at + 22), U16(f, at + 10));
        else if (kind == "pai1")
        {
            const uint16_t frames = U16(f, at + 8), textures = U16(f, at + 12), entries = U16(f, at + 14);
            const uint32_t entriesAt = U32(f, at + 16);
            std::string names;
            for (int i = 0; i < entries; i++) names += " " + Name(f, at + U32(f, at + entriesAt + 4 * i), 20);
            line = Fmt("%spai1: %u frames, loop %u, %u textures, %u animated:%s\n", indent.c_str(), frames, f[at + 10], textures, entries, names.c_str());
        }
        else
            line = Fmt("%s%s: %u bytes\n", indent.c_str(), kind.c_str(), size);
        out += line;
        if (kind == "pas1" || kind == "grs1") depth++;
        if ((kind == "pae1" || kind == "gre1") && depth) depth--;
        at += size;
    }
    return out;
}

}
