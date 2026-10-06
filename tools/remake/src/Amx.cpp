#include "Amx.h"

#include <algorithm>
#include <cstdio>

namespace remake
{

AmxInfo AmxInfo::Read(const Bytes& b)
{
    if (b.size() < 56) throw FormatError("AMX: header too short");
    AmxInfo a;
    a.Size = U32(b, 0); a.Magic = U16(b, 4); a.FileVersion = b[6]; a.AmxVersion = b[7]; a.Flags = U16(b, 8); a.DefSize = U16(b, 10);
    if (a.Magic != 0xF1E0) throw FormatError("AMX: magic " + std::to_string(a.Magic) + " is not 0xF1E0");
    a.Cod = U32(b, 12); a.Dat = U32(b, 16); a.Hea = U32(b, 20); a.Stp = U32(b, 24); a.Cip = U32(b, 28);
    a.Publics = U32(b, 32); a.Natives = U32(b, 36); a.Libraries = U32(b, 40);
    if (a.Size > b.size()) throw FormatError("AMX: size " + std::to_string(a.Size) + " past the " + std::to_string(b.size()) + " bytes given");
    if (a.DefSize == 0 || a.Publics > a.Natives || a.Natives > a.Libraries || a.Cod > a.Dat || a.Dat > a.Hea)
        throw FormatError("AMX: header fields out of order");
    return a;
}


std::vector<int32_t> AmxCells(const Bytes& b)
{
    const AmxInfo a = AmxInfo::Read(b);
    const size_t count = (a.Hea - a.Cod) / 4;
    std::vector<int32_t> cells;
    if (!a.Compact())
    {
        for (size_t i = 0; i < count; i++) cells.push_back((int32_t)U32(b, a.Cod + 4 * i));
        return cells;
    }
    size_t at = a.Cod;
    while (cells.size() < count)
    {
        if (at >= a.Size) throw FormatError("AMX: packed code ends before its " + std::to_string(count) + " cells");
        uint32_t c = (b[at] & 0x40) ? ~0u : 0u; // the first group's bit 6 is the sign
        uint8_t byte;
        do { byte = U8(b, at++); c = (c << 7) | (byte & 0x7F); } while (byte & 0x80);
        cells.push_back((int32_t)c);
    }
    if (at != a.Size) throw FormatError("AMX: " + std::to_string(count) + " cells end at byte " + std::to_string(at) + ", not at the script's size " + std::to_string(a.Size));
    return cells;
}

// a name offset, or, as ORAS's scripts store (run 124: 0x0B13A189 in zone 6's script, past its 6440 bytes), a value
// that is no offset into the script (a hash of the name, inferred), shown in hexadecimal
static std::string Name(const Bytes& b, uint32_t at)
{
    if (at >= b.size()) { char hex[16]; snprintf(hex, sizeof hex, "#%08X", at); return hex; }
    return Text(b, at, std::min<size_t>(64, b.size() - at));
}

std::vector<std::pair<uint32_t, std::string>> AmxPublics(const Bytes& b)
{
    const AmxInfo a = AmxInfo::Read(b);
    std::vector<std::pair<uint32_t, std::string>> out;
    for (uint32_t i = 0; i < a.PublicCount(); i++) out.push_back({U32(b, a.Publics + i * a.DefSize), Name(b, U32(b, a.Publics + i * a.DefSize + 4))});
    return out;
}

std::vector<std::string> AmxNatives(const Bytes& b)
{
    const AmxInfo a = AmxInfo::Read(b);
    std::vector<std::string> out;
    for (uint32_t i = 0; i < a.NativeCount(); i++) out.push_back(Name(b, U32(b, a.Natives + i * a.DefSize + 4)));
    return out;
}

// The opcodes of ORAS's scripts: Pawn 3.3's numbering (amx.h), checked against the game's interpreter as pk3DS
// (github.com/kwsch/pk3DS, pk3DS.Core/Structures/Scripts.cs, ParseScript) decodes it: the same operand count for
// every opcode up to 0x89; 0x7C-0x80 and 0x88, Pawn's debug opcodes file/line/symbol/srange/jump.pri/symtag that Pawn
// 3.3 dropped, refused; 0x8A-0x9D its macro instructions push2..push5 (2-5 operands), load.both, load.s.both, const,
// const.s (2); 0xA2-0xD4 its 51 packed instructions load.p.pri .. push.p.adr, operand in the cell's high 16 bits, and
// pk3DS checks the operands of the global and the local (.s) variants of load/lref/stor/sref/inc/dec as such, opcode
// for opcode. 0x9E-0xA1 have no Pawn 3.3 name known here: named op158..op161 with pk3DS's operand counts (0xA0 is not
// accepted by it). Jump and call targets are relative to the instruction (pk3DS: line * 4 + delta).
// Operands: n cells, -1 casetbl (count, then 1 + 2 x count), -3 op161 (a byte size s, then 2 x (s / 4) + 1), -9 refused
struct Op { const char* Name; int Operands; };
static const Op Ops[] = {
    {"none", -9}, {"load.pri", 1}, {"load.alt", 1}, {"load.s.pri", 1}, {"load.s.alt", 1}, {"lref.pri", 1}, {"lref.alt", 1},
    {"lref.s.pri", 1}, {"lref.s.alt", 1}, {"load.i", 0}, {"lodb.i", 1}, {"const.pri", 1}, {"const.alt", 1}, {"addr.pri", 1},
    {"addr.alt", 1}, {"stor.pri", 1}, {"stor.alt", 1}, {"stor.s.pri", 1}, {"stor.s.alt", 1}, {"sref.pri", 1}, {"sref.alt", 1},
    {"sref.s.pri", 1}, {"sref.s.alt", 1}, {"stor.i", 0}, {"strb.i", 1}, {"lidx", 0}, {"lidx.b", 1}, {"idxaddr", 0},
    {"idxaddr.b", 1}, {"align.pri", 1}, {"align.alt", 1}, {"lctrl", 1}, {"sctrl", 1}, {"move.pri", 0}, {"move.alt", 0},
    {"xchg", 0}, {"push.pri", 0}, {"push.alt", 0}, {"push.r", 1}, {"push.c", 1}, {"push", 1}, {"push.s", 1}, {"pop.pri", 0},
    {"pop.alt", 0}, {"stack", 1}, {"heap", 1}, {"proc", 0}, {"ret", 0}, {"retn", 0}, {"call", 1}, {"call.pri", -9},
    {"jump", 1}, {"jrel", 1}, {"jzer", 1}, {"jnz", 1}, {"jeq", 1}, {"jneq", 1}, {"jless", 1}, {"jleq", 1}, {"jgrtr", 1},
    {"jgeq", 1}, {"jsless", 1}, {"jsleq", 1}, {"jsgrtr", 1}, {"jsgeq", 1}, {"shl", 0}, {"shr", 0}, {"sshr", 0},
    {"shl.c.pri", 1}, {"shl.c.alt", 1}, {"shr.c.pri", 1}, {"shr.c.alt", 1}, {"smul", 0}, {"sdiv", 0}, {"sdiv.alt", 0},
    {"umul", 0}, {"udiv", 0}, {"udiv.alt", 0}, {"add", 0}, {"sub", 0}, {"sub.alt", 0}, {"and", 0}, {"or", 0}, {"xor", 0},
    {"not", 0}, {"neg", 0}, {"invert", 0}, {"add.c", 1}, {"smul.c", 1}, {"zero.pri", 0}, {"zero.alt", 0}, {"zero", 1},
    {"zero.s", 1}, {"sign.pri", 0}, {"sign.alt", 0}, {"eq", 0}, {"neq", 0}, {"less", 0}, {"leq", 0}, {"grtr", 0}, {"geq", 0},
    {"sless", 0}, {"sleq", 0}, {"sgrtr", 0}, {"sgeq", 0}, {"eq.c.pri", 1}, {"eq.c.alt", 1}, {"inc.pri", 0}, {"inc.alt", 0},
    {"inc", 1}, {"inc.s", 1}, {"inc.i", 0}, {"dec.pri", 0}, {"dec.alt", 0}, {"dec", 1}, {"dec.s", 1}, {"dec.i", 0},
    {"movs", 1}, {"cmps", 1}, {"fill", 1}, {"halt", 1}, {"bounds", 1}, {"sysreq.pri", 0}, {"sysreq.c", 1}, {"file", -9},
    {"line", -9}, {"symbol", -9}, {"srange", -9}, {"jump.pri", -9}, {"switch", 1}, {"casetbl", -1}, {"swap.pri", 0},
    {"swap.alt", 0}, {"push.adr", 1}, {"nop", 0}, {"sysreq.n", 2}, {"symtag", -9}, {"break", 0},
    // 138: macro instructions
    {"push2.c", 2}, {"push2", 2}, {"push2.s", 2}, {"push2.adr", 2}, {"push3.c", 3}, {"push3", 3}, {"push3.s", 3},
    {"push3.adr", 3}, {"push4.c", 4}, {"push4", 4}, {"push4.s", 4}, {"push4.adr", 4}, {"push5.c", 5}, {"push5", 5},
    {"push5.s", 5}, {"push5.adr", 5}, {"load.both", 2}, {"load.s.both", 2}, {"const", 2}, {"const.s", 2},
    // 158: no Pawn 3.3 name known here
    {"op158", 1}, {"op159", 0}, {"op160", -9}, {"op161", -3},
    // 162: packed instructions, the operand in the high 16 bits
    {"load.p.pri", 0}, {"load.p.alt", 0}, {"load.p.s.pri", 0}, {"load.p.s.alt", 0}, {"lref.p.pri", 0}, {"lref.p.alt", 0},
    {"lref.p.s.pri", 0}, {"lref.p.s.alt", 0}, {"lodb.p.i", 0}, {"const.p.pri", 0}, {"const.p.alt", 0}, {"addr.p.pri", 0},
    {"addr.p.alt", 0}, {"stor.p.pri", 0}, {"stor.p.alt", 0}, {"stor.p.s.pri", 0}, {"stor.p.s.alt", 0}, {"sref.p.pri", 0},
    {"sref.p.alt", 0}, {"sref.p.s.pri", 0}, {"sref.p.s.alt", 0}, {"strb.p.i", 0}, {"lidx.p.b", 0}, {"idxaddr.p.b", 0},
    {"align.p.pri", 0}, {"align.p.alt", 0}, {"push.p.c", 0}, {"push.p", 0}, {"push.p.s", 0}, {"stack.p", 0}, {"heap.p", 0},
    {"shl.p.c.pri", 0}, {"shl.p.c.alt", 0}, {"shr.p.c.pri", 0}, {"shr.p.c.alt", 0}, {"add.p.c", 0}, {"smul.p.c", 0},
    {"zero.p", 0}, {"zero.p.s", 0}, {"eq.p.c.pri", 0}, {"eq.p.c.alt", 0}, {"inc.p", 0}, {"inc.p.s", 0}, {"dec.p", 0},
    {"dec.p.s", 0}, {"movs.p", 0}, {"cmps.p", 0}, {"fill.p", 0}, {"halt.p", 0}, {"bounds.p", 0}, {"push.p.adr", 0},
};
static const int FirstPacked = 162;
static const size_t OpCount = sizeof Ops / sizeof Ops[0];
static_assert(sizeof Ops / sizeof Ops[0] == 213, "opcodes 0..0xD4");

static bool IsJump(int op) { return op == 49 || (op >= 51 && op <= 64) || op == 129; } // call, jump..jsgeq, switch

std::string AmxDisassemble(const Bytes& b)
{
    const AmxInfo a = AmxInfo::Read(b);
    const std::vector<int32_t> cells = AmxCells(b);
    const std::vector<std::string> natives = AmxNatives(b);
    std::string out;
    char line[200];
    for (const auto& [address, name] : AmxPublics(b)) { snprintf(line, sizeof line, "public %s at 0x%X\n", name.c_str(), address); out += line; }
    const size_t codeCells = a.CodeBytes() / 4;
    std::vector<bool> start(codeCells + 1, false), proc(codeCells + 1, false);
    std::vector<std::pair<size_t, long>> targets; // (instruction, target byte address) of calls and jumps
    size_t unknown = 0;
    for (size_t i = 0; i < codeCells;)
    {
        const uint32_t c = (uint32_t)cells[i];
        const int op = (int)(c & 0xFFFF);
        start[i] = true;
        snprintf(line, sizeof line, "%06zX:", i * 4);
        out += line;
        if ((size_t)op >= OpCount || Ops[op].Operands == -9 || ((c >> 16) && op < FirstPacked))
        {
            snprintf(line, sizeof line, " ?%08X\n", c);
            out += line; unknown++; i++; continue;
        }
        out += " "; out += Ops[op].Name;
        if (op == 46) proc[i] = true;
        if (op >= FirstPacked) { snprintf(line, sizeof line, " %d", (int16_t)(c >> 16)); out += line; }
        int n = Ops[op].Operands;
        if (n == -1) n = i + 1 < codeCells ? 2 + 2 * cells[i + 1] : 0;
        else if (n == -3) n = i + 1 < codeCells ? 2 + 2 * (cells[i + 1] / 4) : 0;
        if (n < 0) n = 0;
        for (int k = 1; k <= n && i + k < codeCells; k++) { snprintf(line, sizeof line, " %d", cells[i + k]); out += line; }
        if (IsJump(op) && i + 1 < codeCells)
        {
            const long target = (long)(i * 4) + cells[i + 1];
            targets.push_back({i, target});
            snprintf(line, sizeof line, "  ; -> %06lX", target);
            out += line;
        }
        if ((op == 123 || op == 135) && i + 1 < codeCells && cells[i + 1] >= 0 && (size_t)cells[i + 1] < natives.size()) out += "  ; " + natives[cells[i + 1]];
        out += "\n";
        i += 1 + (size_t)n;
    }
    // the checks of the table on this script: every call lands on a proc, every jump on an instruction
    size_t callsOk = 0, calls = 0, jumpsOk = 0;
    for (const auto& [at, target] : targets)
    {
        const bool inside = target >= 0 && target % 4 == 0 && (size_t)target / 4 < codeCells;
        if ((cells[at] & 0xFFFF) == 49) { calls++; if (inside && proc[(size_t)target / 4]) callsOk++; }
        else if (inside && start[(size_t)target / 4]) jumpsOk++;
    }
    snprintf(line, sizeof line, "%zu code cells, %zu not an opcode, %zu natives, %zu data cells; %zu of %zu calls land on a proc, %zu of %zu jumps on an instruction\n",
             codeCells, unknown, natives.size(), cells.size() - codeCells, callsOk, calls, jumpsOk, targets.size() - calls);
    return out + line;
}

}
