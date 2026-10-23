// X/Y's shared scripts (OrasXySharedScripts.h): their ids moved, a script's dispatch and objectencount.amx's flags edited
#include "OrasXySharedScripts.h"
#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

int main()
{
    const std::vector<XyScriptRange> rows = {{10480, 10529, 39, 1, 142}, {20000, 20009, 0, 1, 423}};
    XyScriptIds ids;
    ids.Add(rows[0]);
    check(ids.ToOras(10480) == 40480 && ids.ToOras(10529) == 40529, "a ported range's ids moved by 30000");
    check(ids.ToOras(10479) == 10479 && ids.ToOras(20000) == 20000 && ids.ToOras(3005) == 3005, "other ids kept");

    const std::string source =
        "    casetbl 3 L1 -1 L2 10480 L3 20001 L4\n"
        "L5:\n"
        "    load.p.s.pri 12\n    add.p.c 3152\n    stor.p.s.pri -4\n"
        "    push.p.s -4\n    sysreq.n 36 4   ; FlagGet\n"
        "    push.p.s -4\n    sysreq.n 37 4   ; BuildMdlGetHandleByUniqID\n"
        "    push.p.s -4\n    sysreq.n 40 4   ; FlagReset\n";
    const std::string edited = XySharedScriptEdits(source, 39, rows);
    check(edited.find("    casetbl 3 L1 -1 L2 40480 L3 20001 L4\n") == 0, "the dispatch's values in its own ranges moved, the others kept");
    check(edited.find("    load.p.s.pri -4\n    add.p.c 3328\n    push.pri\n    sysreq.n 36 4   ; FlagGet") != std::string::npos &&
          edited.find("    load.p.s.pri -4\n    add.p.c 3328\n    push.pri\n    sysreq.n 40 4   ; FlagReset") != std::string::npos,
          "objectencount.amx: FlagGet and FlagReset given the flag + 0xD00");
    check(edited.find("    push.p.s -4\n    sysreq.n 37 4   ; BuildMdlGetHandleByUniqID") != std::string::npos && edited.find("add.p.c 3152") != std::string::npos,
          "the prop's id (the same value) kept");
    bool refused = false;
    try { XySharedScriptEdits("    push.p.s -4\n    sysreq.n 36 4   ; FlagGet\n", 39, rows); } catch (const FormatError&) { refused = true; }
    check(refused, "objectencount.amx without its two flag calls is refused");
    check(XySharedScriptEdits(source, 7, rows) == "    casetbl 3 L1 -1 L2 10480 L3 20001 L4\n" + source.substr(source.find("L5:")),
          "another member: nothing of its own moved");
    return ok ? 0 : 1;
}
