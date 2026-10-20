// X/Y's natives given to Omega Ruby (OrasXyNatives.h): the assembler's branches and calls, the table and the routine
// XyNativesPatchAt writes, and the native check knowing them under bit 0x800
#include "OrasXyNatives.h"
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
    // a forward beq over one word, a call of 0x3FE5C8 through the pool after the code
    XyNativeAsm a;
    const int end = a.NewLabel();
    a.Branch(0x0, end);
    a.Op(0xE1A00000);
    a.Call(0x3FE5C8);
    a.Set(end);
    a.Op(0xE12FFF1E);
    const std::vector<uint32_t> w = a.Finish();
    check(w.size() == 6, "code and pool: 6 words");
    check(w[0] == 0x0A000002, "beq to word 4 (pc + 8 + 8)");
    check(w[2] == 0xE59FC004, "ldr ip, [pc, #4] reaches the pool's word 5 (pc: word 2 + 8 bytes)");
    check(w[3] == 0xE12FFF3C && w[5] == 0x3FE5C8, "blx ip, the address in the pool");

    const uint32_t cave = 0x10332400;
    const XyNativesPatch p = XyNativesPatchAt(cave);
    const auto at = [](const std::vector<CodeWord>& v, uint32_t address) -> const CodeWord* {
        for (const CodeWord& c : v) if (c.Address == address) return &c;
        return nullptr;
    };
    const CodeWord* test = at(p.Code, 0x3FBDA0);
    check(test && test->Expected == 0xE3140B01 && test->Value == 0xEA05F836, "the bit 0x400 test: b 0x579E80");
    const CodeWord* back = at(p.Code, 0x579EF0);
    check(back && back->Value == (0xEA000000u | (((0x3FBDA4u - 0x579EF0u - 8) >> 2) & 0xFFFFFF)), "the routine's end: b 0x3FBDA4");
    const CodeWord* offset = at(p.Code, 0x579EF4);
    check(offset && offset->Value == cave - 0x1029F1E0u, "the routine's literal: the table from DllField's getter");
    const CodeWord* stub = at(p.Code, 0x579EF8);
    check(stub && stub->Value == 0x19C8D4, "the routine's second literal: the stub's word holding the getter");
    const CodeWord* count = at(p.Field, cave);
    check(count && count->Value == XyNatives().size(), "the table's count");
    bool zeros = true, inCave = true;
    for (const CodeWord& c : p.Field) { zeros = zeros && c.Expected == 0; inCave = inCave && c.Address >= cave; }
    for (const CodeWord& c : p.Code) if (c.Address != 0x3FBDA0) zeros = zeros && c.Expected == 0;
    check(zeros && inCave, "every word but the branch over a zero, DllField's from the cave on");
    const CodeWord* name = at(p.Field, cave + 4);
    check(name && name->Value + 0x1029F1E0u == cave + 4 + 8 * (uint32_t)XyNatives().size(), "the first name follows the table");
    return ok ? 0 : 1;
}
