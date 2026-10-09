// Zone scripts (Amx.h): a compact AMX script written here with Pawn's encoding is unpacked to its cells, its natives
// named and its code disassembled with Pawn 3 opcodes. No game data.
#include "Amx.h"

#include <cstdio>
#include <string>

using namespace remake;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

static void Put32(Bytes& b, size_t at, uint32_t v) { for (int k = 0; k < 4; k++) b[at + k] = (uint8_t)(v >> (8 * k)); }

// Pawn's compact encoding of one cell: 7-bit groups, most significant first, as few as keep the sign
static void Pack(Bytes& out, int32_t v)
{
    uint8_t groups[5];
    int n = 0;
    uint32_t u = (uint32_t)v;
    do { groups[n++] = u & 0x7F; u = (uint32_t)((int32_t)u >> 7); }
    while (!((u == 0 && !(groups[n - 1] & 0x40)) || (u == 0xFFFFFFFF && (groups[n - 1] & 0x40))));
    for (int k = n - 1; k >= 0; k--) out.push_back((uint8_t)(groups[k] | (k ? 0x80 : 0)));
}

int main()
{
    // code: push.c 5; sysreq.c 0 (native 0); stack 4; const.pri -200000; retn  |  data: 7
    const int32_t cells[] = {39, 5, 123, 0, 44, 4, 11, -200000, 48, 7};
    Bytes b(56 + 8 + 8, 0);
    const size_t natives = 56, names = 64;
    b.resize(names);
    for (char c : std::string("MoveObj\0", 8)) b.push_back((uint8_t)c);
    const size_t cod = b.size();
    for (int32_t c : cells) Pack(b, c);
    Put32(b, 0, (uint32_t)b.size());
    b[4] = 0xE0; b[5] = 0xF1; b[6] = 10; b[7] = 10; b[8] = 0x04; b[10] = 8;
    Put32(b, 12, (uint32_t)cod); Put32(b, 16, (uint32_t)cod + 36); Put32(b, 20, (uint32_t)cod + 40); Put32(b, 24, (uint32_t)cod + 40);
    Put32(b, 32, (uint32_t)natives); Put32(b, 36, (uint32_t)natives); Put32(b, 40, (uint32_t)natives + 8);
    Put32(b, natives + 4, (uint32_t)names);
    const std::vector<int32_t> got = AmxCells(b);
    check(got == std::vector<int32_t>(cells, cells + 10), "compact cells unpacked, negative ones sign-extended");
    check(AmxNatives(b) == std::vector<std::string>{"MoveObj"}, "native named");
    const std::string dis = AmxDisassemble(b);
    check(dis.find("push.c 5") != std::string::npos && dis.find("sysreq.c 0  ; MoveObj") != std::string::npos
          && dis.find("const.pri -200000") != std::string::npos && dis.find("0 not an opcode") != std::string::npos,
          "disassembled with Pawn 3 opcodes and native names");
    // casetbl: count, default, then count (value, address) pairs, and the next instruction read after them
    {
        const int32_t sw[] = {129, 8, 130, 1, 100, 5, 200, 48};  // switch; casetbl 1 default 100 (5 -> 200); retn
        Bytes c(b.begin(), b.begin() + (long)cod);
        for (int32_t v : sw) Pack(c, v);
        Pack(c, 7);
        Put32(c, 0, (uint32_t)c.size()); Put32(c, 16, (uint32_t)cod + 32); Put32(c, 20, (uint32_t)cod + 36); Put32(c, 24, (uint32_t)cod + 36);
        const std::string d = AmxDisassemble(c);
        check(d.find("casetbl 1 100 5 200\n") != std::string::npos && d.find("retn") != std::string::npos, "casetbl takes 2 + 2 x count operands");
    }
    // packed instructions and a relative call: proc; push.p.c 8 (0x800BC); load.p.s.pri -4 (0xFFFC00A4); retn; then
    // call -16 at byte 16, back to the proc at byte 0; zero.pri
    {
        const int32_t code[] = {46, 0x800BC, (int32_t)0xFFFC00A4, 48, 49, -16, 89};
        Bytes c(b.begin(), b.begin() + (long)cod);
        for (int32_t v : code) Pack(c, v);
        Pack(c, 7);
        Put32(c, 0, (uint32_t)c.size()); Put32(c, 16, (uint32_t)cod + 28); Put32(c, 20, (uint32_t)cod + 32); Put32(c, 24, (uint32_t)cod + 32);
        const std::string d = AmxDisassemble(c);
        check(d.find("push.p.c 8") != std::string::npos && d.find("load.p.s.pri -4") != std::string::npos, "packed instructions named, operand in the high 16 bits");
        check(d.find("call -16  ; -> 000000") != std::string::npos && d.find("1 of 1 calls land on a proc") != std::string::npos, "a relative call checked to land on a proc");
    }

    // the writer (AmxScript, AmxAssemble, AmxSource): a zone script written by hand as the game's smallest one is laid
    // out (zone 80: main dispatches on the public variable the game sets, data cell 0), natives named, a switch by labels
    {
        const std::string source =
            "pubvar #865A53E1 8\npubvar #29654047 4\npubvar #D7477C97 command\n"
            "code\n    halt.p 0\nmain:\n    proc\n    load.p.pri 0\n    switch cases\n"
            "enter:\n    sysreq.n _FieldMapBlockIsLoading 0\n    jump done\n"
            "other:\n    push.c -200000000\n    sysreq.n CommandNOP 4\n    stack 4\n    jump done\n"
            "cases:\n    casetbl 1 other -1 enter\n"
            "done:\n    zero.pri\n    retn\n"
            "data\ncommand:\n    cell -1 0 0\n";
        std::vector<std::string> asked;
        const Bytes script = AmxAssemble("header 10 10 0x1C main 4096\n" + source, [&](const std::string& n) { asked.push_back(n); return std::string(); });
        check(asked == std::vector<std::string>{"_FieldMapBlockIsLoading", "CommandNOP"}, "natives named are checked, in order of use");
        check(AmxNameHash("_Suspend") == 0x0B13A389 && AmxNameHash("Float") == 0xCA18AA2E, "names hashed as the game links them");
        const AmxScript s = AmxScript::Read(script);
        check(s.Natives.size() == 2 && s.Natives[1].Name == AmxNameHash("CommandNOP") && s.Cip == 4, "natives declared on first use, cip on main");
        check(s.Libraries.size() == 1 && s.Tags.size() == 1 && s.Tags[0].Address == 0x40000002 && s.NameTable == Bytes{0x3F, 0, 0, 0},
              "the game's library, tag and name table by default");
        check(s.PublicVars.size() == 3 && s.PublicVars[2].Address == 0 && s.Data == std::vector<int32_t>{-1, 0, 0}, "public variables on data labels");
        const std::string d = AmxDisassemble(script);
        check(d.find("0 not an opcode") != std::string::npos && d.find("3 of 3 jumps") != std::string::npos, "the script passes the disassembler's checks");
        check(d.find("push.c -200000000") != std::string::npos, "a cell of five groups packs and unpacks");
        // casetbl at 0x4C (cell 19): its default, `other` at 0x28, in cell 0x54 relative to 0x50; the case -1, `enter` at
        // 0x14, in cell 0x5C relative to 0x58
        check(s.Code[19] == 130 && s.Code[21] == 0x28 - 0x50 && s.Code[23] == 0x14 - 0x58, "casetbl addresses relative to the cell before each");
        check(AmxAssemble(AmxSource(script)) == script, "its source assembles back to the same bytes");
        check(AmxScript::Read(script).Write() == script, "read and written back to the same bytes");
        bool refused = false;
        try { AmxAssemble("code\n    sysreq.n NoSuchNative 0\n", [](const std::string&) { return std::string("not in the game's tables"); }); } catch (const FormatError&) { refused = true; }
        check(refused, "an unknown native refused");
        refused = false;
        try { AmxAssemble("code\n    push.c\n"); } catch (const FormatError& e) { refused = std::string(e.what()).find("line 2") != std::string::npos; }
        check(refused, "a wrong operand count refused, naming the line");
    }
    Bytes truncated = b;
    truncated.push_back(0); Put32(truncated, 0, (uint32_t)truncated.size());
    bool threw = false;
    try { AmxCells(truncated); } catch (const FormatError&) { threw = true; }
    check(threw, "packed bytes not ending at the script's size refused");
    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
