#include "Amx.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>

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
// Checked on the whole game (run 126, `oras-script all`): 1072 scripts, 569031 code cells, none that is no opcode,
// 40720 of 40720 calls landing on a proc, 33478 of 33478 jumps on an instruction.
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

// the operand cells of the instruction at cell i, as the table gives them (casetbl and op161: from their first operand)
static size_t OperandCells(const std::vector<int32_t>& cells, size_t i, size_t codeCells)
{
    const int op = (int)((uint32_t)cells[i] & 0xFFFF);
    int n = Ops[op].Operands;
    if (n == -1) n = i + 1 < codeCells ? 2 + 2 * cells[i + 1] : 0;
    else if (n == -3) n = i + 1 < codeCells ? 2 + 2 * (cells[i + 1] / 4) : 0;
    return n < 0 ? 0 : (size_t)n;
}

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
        const int n = (int)OperandCells(cells, i, codeCells);
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


uint32_t AmxNameHash(const std::string& name)
{
    uint32_t h = 0;
    for (char c : name) h = h * 0x83 ^ (uint32_t)(int32_t)(signed char)c;
    return h;
}

static void Put32(Bytes& b, uint32_t v) { for (int k = 0; k < 4; k++) b.push_back((uint8_t)(v >> (8 * k))); }

// Pawn's compact encoding of one cell, as its compiler writes it (pawncc sc6.c, write_encoded): the five 7-bit groups by an
// unsigned shift (the top group holds 4 bits: 0x0F for a negative cell, not 0x7F), leading zero groups dropped while the
// next keeps the sign clear, a leading 0x0F then 0x7F groups dropped while the next keeps it set; most significant first,
// every byte but the last with bit 7 set. Checked: every zone script packs back to its bytes (`oras-script all`)
static void Pack(Bytes& out, int32_t v)
{
    uint8_t t[5];
    uint32_t u = (uint32_t)v;
    for (int k = 0; k < 5; k++) { t[k] = u & 0x7F; u >>= 7; }
    int n = 5;
    while (n > 1 && t[n - 1] == 0 && !(t[n - 2] & 0x40)) n--;
    if (n == 5 && t[4] == 0x0F && (t[3] & 0x40)) n--;
    while (n > 1 && t[n - 1] == 0x7F && (t[n - 2] & 0x40)) n--;
    for (int k = n - 1; k >= 0; k--) out.push_back((uint8_t)(t[k] | (k ? 0x80 : 0)));
}

static const size_t HeaderBytes = 60;

AmxScript AmxScript::Read(const Bytes& b)
{
    const AmxInfo a = AmxInfo::Read(b);
    if (b.size() < HeaderBytes) throw FormatError("AMX: header too short");
    const uint32_t pubvars = U32(b, 44), tags = U32(b, 48), names = U32(b, 52);
    if (a.DefSize != 8 || a.Publics != HeaderBytes || pubvars < a.Libraries || tags < pubvars || names < tags || a.Cod < names)
        throw FormatError("AMX: tables not laid out as the game's scripts lay them");
    if (U32(b, 56) != names) throw FormatError("AMX: the overlays word is not the name table's offset, as in every zone script");
    AmxScript s;
    s.FileVersion = a.FileVersion; s.AmxVersion = a.AmxVersion; s.Flags = a.Flags; s.DefSize = a.DefSize;
    s.Cip = a.Cip; s.StackBytes = a.Stp - a.Hea;
    const auto table = [&](uint32_t from, uint32_t to) {
        std::vector<Entry> t;
        for (uint32_t at = from; at + 8 <= to; at += 8) t.push_back({U32(b, at), U32(b, at + 4)});
        return t;
    };
    s.Publics = table(a.Publics, a.Natives); s.Natives = table(a.Natives, a.Libraries); s.Libraries = table(a.Libraries, pubvars);
    s.PublicVars = table(pubvars, tags); s.Tags = table(tags, names);
    s.NameTable.assign(b.begin() + names, b.begin() + a.Cod);
    const std::vector<int32_t> cells = AmxCells(b);
    s.Code.assign(cells.begin(), cells.begin() + a.CodeBytes() / 4);
    s.Data.assign(cells.begin() + a.CodeBytes() / 4, cells.end());
    return s;
}

Bytes AmxScript::Write() const
{
    if (!(Flags & 0x04)) throw FormatError("AMX: only compact scripts are written, as the game's are");
    Bytes b;
    const uint32_t publics = HeaderBytes, natives = publics + 8 * (uint32_t)Publics.size(), libraries = natives + 8 * (uint32_t)Natives.size();
    const uint32_t pubvars = libraries + 8 * (uint32_t)Libraries.size(), tags = pubvars + 8 * (uint32_t)PublicVars.size();
    const uint32_t names = tags + 8 * (uint32_t)Tags.size(), cod = names + (uint32_t)NameTable.size();
    const uint32_t dat = cod + 4 * (uint32_t)Code.size(), hea = dat + 4 * (uint32_t)Data.size();
    Put32(b, 0); // the size, set last
    b.push_back(0xE0); b.push_back(0xF1); b.push_back(FileVersion); b.push_back(AmxVersion);
    b.push_back((uint8_t)Flags); b.push_back((uint8_t)(Flags >> 8)); b.push_back((uint8_t)DefSize); b.push_back((uint8_t)(DefSize >> 8));
    for (uint32_t v : {cod, dat, hea, hea + StackBytes, Cip, publics, natives, libraries, pubvars, tags, names, names}) Put32(b, v);
    for (const auto* t : {&Publics, &Natives, &Libraries, &PublicVars, &Tags})
        for (const Entry& e : *t) { Put32(b, e.Address); Put32(b, e.Name); }
    b.insert(b.end(), NameTable.begin(), NameTable.end());
    for (int32_t c : Code) Pack(b, c);
    for (int32_t c : Data) Pack(b, c);
    for (int k = 0; k < 4; k++) b[k] = (uint8_t)(b.size() >> (8 * k));
    return b;
}

// casetbl's operand j (2, 4...: the default's address, then each case's) as a byte address: relative to the cell before
// it. Read on the game's scripts (zone 80: default -24 from 0x54 lands on its sysreq.n at 0x3C, the cases -52 from 0x5C and
// -80 from 0x64 on 0x28 and 0x14, each an instruction)
static long CaseTarget(const std::vector<int32_t>& code, size_t i, size_t j) { return (long)((i + j - 1) * 4) + code[i + j]; }

static std::string Hash(uint32_t h) { char t[16]; snprintf(t, sizeof t, "#%08X", h); return t; }

std::string AmxSource(const Bytes& b)
{
    const AmxScript s = AmxScript::Read(b);
    const size_t n = s.Code.size();
    // the instructions, and the labels: every jump or call target on an instruction, the cip, the publics
    std::vector<bool> start(n + 1, false), label(n + 1, false);
    for (size_t i = 0; i < n; i += 1 + OperandCells(s.Code, i, n))
    {
        start[i] = true;
        const int op = (int)((uint32_t)s.Code[i] & 0xFFFF);
        if ((size_t)op >= OpCount || Ops[op].Operands == -9) throw FormatError("AMX: a cell that is no opcode, written as no source");
    }
    const auto mark = [&](long at) { if (at >= 0 && at % 4 == 0 && (size_t)at / 4 < n && start[(size_t)at / 4]) label[(size_t)at / 4] = true; };
    for (size_t i = 0; i < n; i += 1 + OperandCells(s.Code, i, n))
    {
        const int op = (int)((uint32_t)s.Code[i] & 0xFFFF);
        if (IsJump(op) && i + 1 < n) mark((long)(i * 4) + s.Code[i + 1]);
        if (op == 130) for (size_t j = 2; j <= OperandCells(s.Code, i, n) && i + j < n; j += 2) mark(CaseTarget(s.Code, i, j));
    }
    mark((long)s.Cip);
    for (const auto& e : s.Publics) mark((long)e.Address);
    const auto at = [&](uint32_t a) { char t[16]; if (a % 4 == 0 && a / 4 < n && label[a / 4]) { snprintf(t, sizeof t, "L%06X", a); return std::string(t); } return std::to_string(a); };

    std::string out;
    char line[64];
    snprintf(line, sizeof line, "header %u %u 0x%X %s %u\n", s.FileVersion, s.AmxVersion, s.Flags, at(s.Cip).c_str(), s.StackBytes);
    out += line;
    for (const auto& e : s.Publics) out += "public " + Hash(e.Name) + " " + at(e.Address) + "\n";
    for (const auto& e : s.Natives) out += "native " + Hash(e.Name) + (e.Address ? " " + std::to_string(e.Address) : "") + "\n";
    for (const auto& e : s.Libraries) out += "library " + Hash(e.Name) + " " + std::to_string(e.Address) + "\n";
    for (const auto& e : s.PublicVars) out += "pubvar " + Hash(e.Name) + " " + std::to_string(e.Address) + "\n";
    for (const auto& e : s.Tags) { snprintf(line, sizeof line, "tag %s 0x%X\n", Hash(e.Name).c_str(), e.Address); out += line; }
    out += "nametable ";
    for (uint8_t c : s.NameTable) { snprintf(line, sizeof line, "%02x", c); out += line; }
    out += "\ncode\n";
    for (size_t i = 0; i < n;)
    {
        const uint32_t c = (uint32_t)s.Code[i];
        const int op = (int)(c & 0xFFFF);
        if (label[i]) { snprintf(line, sizeof line, "L%06zX:\n", i * 4); out += line; }
        out += "    "; out += Ops[op].Name;
        if (op >= FirstPacked) out += " " + std::to_string((int16_t)(c >> 16));
        else if (c >> 16) throw FormatError("AMX: an unpacked opcode with high bits set, written as no source");
        const size_t k = OperandCells(s.Code, i, n);
        for (size_t j = 1; j <= k && i + j < n; j++)
        {
            const long target = (long)(i * 4) + s.Code[i + j];
            const long caseTarget = op == 130 && j % 2 == 0 ? CaseTarget(s.Code, i, j) : -1;
            if (j == 1 && IsJump(op) && target >= 0 && target % 4 == 0 && (size_t)target / 4 < n && label[(size_t)target / 4]) out += " " + at((uint32_t)target);
            else if (caseTarget >= 0 && caseTarget % 4 == 0 && (size_t)caseTarget / 4 < n && label[(size_t)caseTarget / 4]) out += " " + at((uint32_t)caseTarget);
            else out += " " + std::to_string(s.Code[i + j]);
        }
        out += "\n";
        i += 1 + k;
    }
    out += "data\n";
    for (size_t i = 0; i < s.Data.size(); i++) out += (i % 8 ? " " : "    cell ") + std::to_string(s.Data[i]) + (i % 8 == 7 || i + 1 == s.Data.size() ? "\n" : "");
    return out;
}

Bytes AmxAssemble(const std::string& source, const std::function<bool(const std::string&)>& knownNative)
{
    struct Line { size_t Number; std::vector<std::string> Words; };
    std::vector<Line> lines;
    {
        std::istringstream in(source);
        std::string text;
        for (size_t number = 1; std::getline(in, text); number++)
        {
            text = text.substr(0, text.find(';'));
            std::istringstream words(text);
            Line l{number, {}};
            for (std::string w; words >> w;) l.Words.push_back(w);
            if (!l.Words.empty()) lines.push_back(l);
        }
    }
    std::map<std::string, int> opcode;
    for (size_t i = 0; i < OpCount; i++) if (Ops[i].Operands != -9) opcode[Ops[i].Name] = (int)i;
    const auto fail = [](const Line& l, const std::string& why) { return FormatError("AMX source line " + std::to_string(l.Number) + ": " + why); };
    const auto number = [](const std::string& w, long& v) {
        try { size_t used; v = std::stol(w, &used, 0); return used == w.size(); } catch (...) { return false; }
    };

    // first pass: the byte address of every label, code and data
    std::map<std::string, uint32_t> codeLabels, dataLabels;
    {
        int section = 0; // 0 none, 1 code, 2 data
        uint32_t codeAt = 0, dataAt = 0;
        for (const Line& l : lines)
        {
            const std::string& w = l.Words[0];
            if (w == "code") section = 1;
            else if (w == "data") section = 2;
            else if (w.back() == ':' && l.Words.size() == 1)
            {
                if (section == 0) throw fail(l, "a label before code or data");
                const std::string name = w.substr(0, w.size() - 1);
                if (codeLabels.count(name) || dataLabels.count(name)) throw fail(l, "label " + name + " given twice");
                (section == 1 ? codeLabels : dataLabels)[name] = section == 1 ? codeAt : dataAt;
            }
            else if (section == 1) codeAt += 4 * (w == "cell" ? (uint32_t)l.Words.size() - 1 : (uint32_t)l.Words.size() - (opcode.count(w) && opcode[w] >= FirstPacked ? 1 : 0));
            else if (section == 2) { if (w != "cell") throw fail(l, "only cells in data"); dataAt += 4 * ((uint32_t)l.Words.size() - 1); }
        }
    }
    const auto name = [&](const Line& l, const std::string& w) -> uint32_t {
        if (w[0] == '#') { long v; if (!number("0x" + w.substr(1), v)) throw fail(l, "hash " + w + " is not hexadecimal"); return (uint32_t)v; }
        if (knownNative && !knownNative(w)) throw fail(l, "no native " + w + " in the game's tables");
        return AmxNameHash(w);
    };
    const auto value = [&](const Line& l, const std::string& w, bool code) -> uint32_t {
        long v;
        if (number(w, v)) return (uint32_t)v;
        const auto& labels = code ? codeLabels : dataLabels;
        if (!labels.count(w)) throw fail(l, std::string("no ") + (code ? "code" : "data") + " label " + w);
        return labels.at(w);
    };

    AmxScript s;
    s.StackBytes = 0x1000;
    if (codeLabels.count("main")) s.Cip = codeLabels.at("main");
    bool tables = false; // a library, tag or name table given: the defaults are then left out
    int section = 0;
    for (const Line& l : lines)
    {
        const std::vector<std::string>& w = l.Words;
        if (w[0] == "code") { section = 1; continue; }
        if (w[0] == "data") { section = 2; continue; }
        if (w[0].back() == ':' && w.size() == 1) continue;
        if (section == 0)
        {
            if (w[0] == "header" && w.size() == 6)
            {
                long fv, av, fl, st;
                if (!number(w[1], fv) || !number(w[2], av) || !number(w[3], fl) || !number(w[5], st)) throw fail(l, "header: numbers expected");
                s.FileVersion = (uint8_t)fv; s.AmxVersion = (uint8_t)av; s.Flags = (uint16_t)fl; s.StackBytes = (uint32_t)st;
                s.Cip = value(l, w[4], true);
            }
            else if (w[0] == "public" && w.size() == 3) s.Publics.push_back({value(l, w[2], true), name(l, w[1])});
            else if (w[0] == "native" && (w.size() == 2 || w.size() == 3)) s.Natives.push_back({w.size() == 3 ? value(l, w[2], false) : 0, name(l, w[1])});
            else if (w[0] == "library" && w.size() == 3) { tables = true; s.Libraries.push_back({value(l, w[2], false), name(l, w[1])}); }
            else if (w[0] == "pubvar" && w.size() == 3) s.PublicVars.push_back({value(l, w[2], false), name(l, w[1])});
            else if (w[0] == "tag" && w.size() == 3) { tables = true; s.Tags.push_back({value(l, w[2], false), name(l, w[1])}); }
            else if (w[0] == "nametable" && w.size() == 2 && w[1].size() % 2 == 0)
            {
                tables = true;
                for (size_t k = 0; k < w[1].size(); k += 2) { long v; if (!number("0x" + w[1].substr(k, 2), v)) throw fail(l, "nametable: hex bytes expected"); s.NameTable.push_back((uint8_t)v); }
            }
            else throw fail(l, "unknown statement " + w[0]);
            continue;
        }
        std::vector<int32_t>& cells = section == 1 ? s.Code : s.Data;
        if (w[0] == "cell")
        {
            for (size_t k = 1; k < w.size(); k++) cells.push_back((int32_t)value(l, w[k], section == 1));
            continue;
        }
        if (section == 2) throw fail(l, "only cells in data");
        if (!opcode.count(w[0])) throw fail(l, "no opcode " + w[0]);
        const int op = opcode.at(w[0]);
        const uint32_t here = 4 * (uint32_t)s.Code.size();
        if (op >= FirstPacked)
        {
            long v;
            if (w.size() != 2 || !number(w[1], v) || v < -32768 || v > 32767) throw fail(l, w[0] + " takes one 16-bit operand");
            s.Code.push_back((int32_t)((uint32_t)op | (uint32_t)(v & 0xFFFF) << 16));
            continue;
        }
        s.Code.push_back(op);
        for (size_t k = 1; k < w.size(); k++)
        {
            long v;
            if (k == 1 && (op == 123 || op == 135) && !number(w[k], v))
            {
                // a native by name or hash: its index, declared on first use
                const uint32_t h = name(l, w[k]);
                size_t index = 0;
                while (index < s.Natives.size() && s.Natives[index].Name != h) index++;
                if (index == s.Natives.size()) s.Natives.push_back({0, h});
                s.Code.push_back((int32_t)index);
            }
            else if (k == 1 && IsJump(op) && !number(w[k], v)) s.Code.push_back((int32_t)(value(l, w[k], true) - here));
            else if (op == 130 && k % 2 == 0 && !number(w[k], v)) s.Code.push_back((int32_t)(value(l, w[k], true) - (here + 4 * ((uint32_t)k - 1))));
            else if (number(w[k], v)) s.Code.push_back((int32_t)v);
            else throw fail(l, "operand " + w[k] + " is no number");
        }
        const size_t i = s.Code.size() - w.size(), wanted = OperandCells(s.Code, i, s.Code.size());
        if (w.size() - 1 != wanted) throw fail(l, w[0] + " takes " + std::to_string(wanted) + " operands, not " + std::to_string(w.size() - 1));
    }
    if (!tables)
    {
        s.Libraries.push_back({0, AmxNameHash("Float")});
        s.Tags.push_back({0x40000002, AmxNameHash("Float")});
        s.NameTable = {0x3F, 0, 0, 0};
    }
    if (s.Code.empty()) throw fail(lines.empty() ? Line{0, {}} : lines.back(), "no code");
    return s.Write();
}

}
