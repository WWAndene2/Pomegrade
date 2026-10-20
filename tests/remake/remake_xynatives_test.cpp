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
    const auto bl = [](uint32_t from, uint32_t to) { return 0xEB000000u | (((to - from - 8) >> 2) & 0xFFFFFF); };
    const CodeWord* test = at(p.Code, 0x3FBDA0);
    check(test && test->Expected == 0xE3140B01 && test->Value == bl(0x3FBDA0, 0x579E80), "the bit 0x400 test: bl the first stub");
    const CodeWord* tail = at(p.Code, 0x506178);
    check(tail && tail->Expected == bl(0x506178, 0x19C910) && tail->Value == bl(0x506178, 0x579E98),
          "Script_LinkNativeImports' getter call: bl the second stub");
    const CodeWord* stub = at(p.Code, 0x579EC0);
    check(stub && stub->Value == 0x19C8D4, "the stubs' literal: the import stub's word holding the getter");
    const CodeWord* toSub = at(p.Code, 0x579EC4);
    const CodeWord* subLiteral = toSub ? at(p.Field, toSub->Value + 0x1029F1E0u + 0x74) : nullptr;
    check(subLiteral && subLiteral->Value + toSub->Value + 0x1029F1E0u + 0x14 == cave,
          "the subroutine, from the getter, finds the table from its pc");
    const CodeWord* count = at(p.Field, cave);
    check(count && count->Value == XyNatives().size(), "the table's count");
    bool zeros = true, inCave = true;
    for (const CodeWord& c : p.Field) { zeros = zeros && c.Expected == 0; inCave = inCave && c.Address >= cave; }
    for (const CodeWord& c : p.Code) if (c.Address != 0x3FBDA0 && c.Address != 0x506178) zeros = zeros && c.Expected == 0;
    check(zeros && inCave, "every word but the two calls over a zero, DllField's from the cave on");
    const CodeWord* name = at(p.Field, cave + 4);
    check(name && name->Value == 4 + 8 * (uint32_t)XyNatives().size(), "the first name follows the table (offsets from the table)");
    return ok ? 0 : 1;
}
