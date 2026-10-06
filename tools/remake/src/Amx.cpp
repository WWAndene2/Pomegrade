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

// Pawn 3 opcodes (amx.h, OP_LOAD_PRI = 1 ...): name and operand count (-1: CASETBL, -2: FILE/SYMBOL, variable)
struct Op { const char* Name; int Operands; };
static const Op Ops[] = {
    {"none", 0}, {"load.pri", 1}, {"load.alt", 1}, {"load.s.pri", 1}, {"load.s.alt", 1}, {"lref.pri", 1}, {"lref.alt", 1},
    {"lref.s.pri", 1}, {"lref.s.alt", 1}, {"load.i", 0}, {"lodb.i", 1}, {"const.pri", 1}, {"const.alt", 1}, {"addr.pri", 1},
    {"addr.alt", 1}, {"stor.pri", 1}, {"stor.alt", 1}, {"stor.s.pri", 1}, {"stor.s.alt", 1}, {"sref.pri", 1}, {"sref.alt", 1},
    {"sref.s.pri", 1}, {"sref.s.alt", 1}, {"stor.i", 0}, {"strb.i", 1}, {"lidx", 0}, {"lidx.b", 1}, {"idxaddr", 0},
    {"idxaddr.b", 1}, {"align.pri", 1}, {"align.alt", 1}, {"lctrl", 1}, {"sctrl", 1}, {"move.pri", 0}, {"move.alt", 0},
    {"xchg", 0}, {"push.pri", 0}, {"push.alt", 0}, {"push.r", 1}, {"push.c", 1}, {"push", 1}, {"push.s", 1}, {"pop.pri", 0},
    {"pop.alt", 0}, {"stack", 1}, {"heap", 1}, {"proc", 0}, {"ret", 0}, {"retn", 0}, {"call", 1}, {"call.pri", 0},
    {"jump", 1}, {"jrel", 1}, {"jzer", 1}, {"jnz", 1}, {"jeq", 1}, {"jneq", 1}, {"jless", 1}, {"jleq", 1}, {"jgrtr", 1},
    {"jgeq", 1}, {"jsless", 1}, {"jsleq", 1}, {"jsgrtr", 1}, {"jsgeq", 1}, {"shl", 0}, {"shr", 0}, {"sshr", 0},
    {"shl.c.pri", 1}, {"shl.c.alt", 1}, {"shr.c.pri", 1}, {"shr.c.alt", 1}, {"smul", 0}, {"sdiv", 0}, {"sdiv.alt", 0},
    {"umul", 0}, {"udiv", 0}, {"udiv.alt", 0}, {"add", 0}, {"sub", 0}, {"sub.alt", 0}, {"and", 0}, {"or", 0}, {"xor", 0},
    {"not", 0}, {"neg", 0}, {"invert", 0}, {"add.c", 1}, {"smul.c", 1}, {"zero.pri", 0}, {"zero.alt", 0}, {"zero", 1},
    {"zero.s", 1}, {"sign.pri", 0}, {"sign.alt", 0}, {"eq", 0}, {"neq", 0}, {"less", 0}, {"leq", 0}, {"grtr", 0}, {"geq", 0},
    {"sless", 0}, {"sleq", 0}, {"sgrtr", 0}, {"sgeq", 0}, {"eq.c.pri", 1}, {"eq.c.alt", 1}, {"inc.pri", 0}, {"inc.alt", 0},
    {"inc", 1}, {"inc.s", 1}, {"inc.i", 0}, {"dec.pri", 0}, {"dec.alt", 0}, {"dec", 1}, {"dec.s", 1}, {"dec.i", 0},
    {"movs", 1}, {"cmps", 1}, {"fill", 1}, {"halt", 1}, {"bounds", 1}, {"sysreq.pri", 0}, {"sysreq.c", 1}, {"file", -2},
    {"line", 2}, {"symbol", -2}, {"srange", 2}, {"jump.pri", 0}, {"switch", 1}, {"casetbl", -1}, {"swap.pri", 0},
    {"swap.alt", 0}, {"push.adr", 1}, {"nop", 0}, {"sysreq.n", 2}, {"symtag", 1}, {"break", 0},
};

std::string AmxDisassemble(const Bytes& b)
{
    const AmxInfo a = AmxInfo::Read(b);
    const std::vector<int32_t> cells = AmxCells(b);
    const std::vector<std::string> natives = AmxNatives(b);
    std::string out;
    char line[160];
    for (const auto& [address, name] : AmxPublics(b)) { snprintf(line, sizeof line, "public %s at 0x%X\n", name.c_str(), address); out += line; }
    const size_t codeCells = a.CodeBytes() / 4;
    size_t unknown = 0;
    for (size_t i = 0; i < codeCells;)
    {
        const int32_t op = cells[i];
        snprintf(line, sizeof line, "%06zX:", i * 4);
        out += line;
        if (op < 0 || (size_t)op >= sizeof Ops / sizeof Ops[0])
        {
            // a packed instruction (Amx.h): opcode in the low 16 bits, operand in the high 16
            snprintf(line, sizeof line, " p%d %d\n", op & 0xFFFF, (int16_t)(op >> 16));
            out += line; unknown++; i++; continue;
        }
        out += " "; out += Ops[op].Name;
        int n = Ops[op].Operands;
        if (n == -1 && i + 1 < codeCells) n = 2 + 2 * cells[i + 1];            // casetbl: count, default, count (value, address)
        else if (n == -2 && i + 1 < codeCells) n = 1 + cells[i + 1] / 4;       // file/symbol: a size in bytes, then that many
        for (int k = 1; k <= n && i + k < codeCells; k++) { snprintf(line, sizeof line, " %d", cells[i + k]); out += line; }
        if (op == 123 && i + 1 < codeCells && cells[i + 1] >= 0 && (size_t)cells[i + 1] < natives.size()) out += "  ; " + natives[cells[i + 1]];
        out += "\n";
        i += 1 + std::max(n, 0);
    }
    snprintf(line, sizeof line, "%zu code cells, %zu packed (not a Pawn 3 opcode), %zu natives, %zu data cells\n", codeCells, unknown, natives.size(), cells.size() - codeCells);
    return out + line;
}

}
