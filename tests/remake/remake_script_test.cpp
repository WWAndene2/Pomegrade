// Zone scripts (Amx.h): a compact AMX script written here with Pawn's encoding is unpacked to its cells, its natives
// named and its code disassembled with Pawn 3 opcodes. No game data.
#include "Amx.h"
#include "OrasXyScript.h"
#include "OrasXySound.h"
#include "OrasXyTrainers.h"

#include <cstdio>
#include <filesystem>
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
    {
        // an X/Y script's natives made Omega Ruby's (OrasXyScript.h): one renamed, one kept, one missing, against a names
        // file written as function_names.tsv's Script_Native_ rows
        const std::string names = (std::filesystem::temp_directory_path() / "remake_script_test_names.tsv").string();
        const std::string rows =
            "003BD480\tScript_Native__FadeRequestIn\tPawn (AMX) native, named by the game's own native table; \"_FadeRequestIn\" (hash 0x7B2FEC69, table 0x0057A860): x.\tread\n"
            "10293344\tScript_Native_FlagSet\tPawn (AMX) native, named by the game's own native table; \"FlagSet\" (hash 0x13413496, table 0x1033A234, bit 1): x.\tread\n"
            "10293330\tScript_Native_FlagGet\tPawn (AMX) native, named by the game's own native table; \"FlagGet\" (hash 0x133DAA6A, table 0x1033A234, bit 1): x.\tread\n"
            "1029344C\tScript_Native_WorkSet\tPawn (AMX) native, named by the game's own native table; \"WorkSet\" (hash 0x8E55F629, table 0x1033A234, bit 1): x.\tread\n";
        WriteFile(names, Bytes(rows.begin(), rows.end()));
        // CallKisekaeDisplay: an X/Y native nobody provides; ChangeFashionBefore: one the mod provides (OrasXyFashion.h),
        // under the X/Y natives' mask bit 0x800
        const Bytes x = AmxAssemble("code\n    sysreq.n FadeRequestIn 0\n    sysreq.n FlagSet 0\n    sysreq.n CallKisekaeDisplay 0\n    sysreq.n ChangeFashionBefore 0\n    halt 0\n");
        const XyScriptNatives t = XyScriptToOras(x, names, 0x243);
        const AmxScript s = AmxScript::Read(t.Script);
        check(t.Renamed.size() == 1 && t.Renamed[0] == "FadeRequestIn -> _FadeRequestIn", "an X native renamed to Omega Ruby's");
        check(s.Natives.size() == 4 && s.Natives[0].Name == AmxNameHash("_FadeRequestIn") && s.Natives[1].Name == AmxNameHash("FlagSet"),
              "the native table rewritten, the shared native kept");
        check(t.Missing.size() == 2 && t.Missing[0].find("not in Omega Ruby's tables") != std::string::npos, "a native Omega Ruby lacks reported");
        check(t.Missing.size() == 2 && t.Missing[1].find("bit 0x800") != std::string::npos, "an X/Y native the mod provides asks for its mask bit");
        check(XyScriptToOras(x, names, 0x243 | 0x800).Missing.size() == 1, "with that bit, only the native nobody provides is missing");
        check(AmxCells(t.Script) == AmxCells(x), "the code left as it was");
        // the source step: X's flags and works moved, also when a procedure gets the flag as a parameter
        const Bytes y = AmxAssemble(
            "code\n"
            "main:\n"
            "    push.p.c 614\n    sysreq.n FlagSet 4\n"
            "    push.p.c 9\n    push.p.c 16498\n    sysreq.n WorkSet 8\n"
            "    push.p.c 615\n    push.p.c 4\n    call helper\n"
            "    halt 0\n"
            "helper:\n    proc\n    push.p.s 12\n    sysreq.n FlagGet 4\n    retn\n");
        const XyNumbers numbers = XyNumbersFor({y}, {}, {});
        check(numbers.Flags.size() == 2 && numbers.Flags.at(614) == XyFirstFlag + 614 && numbers.Flags.at(615) == XyFirstFlag + 615,
              "X's flags moved by 0xD00, the one passed to a procedure too");
        check(numbers.Works.size() == 1 && numbers.Works.at(16498) == XyFirstWork + 16498 - 0x4000, "X's event work moved to 0x4178 + its rank from 0x4000");
        std::vector<std::string> notes;
        const std::string source = XyScriptSource(y, names, numbers, notes);
        check(source.find("push.p.c " + std::to_string(XyFirstFlag + 614) + "   ; X/Y's flag 614") != std::string::npos
                  && source.find("push.p.c " + std::to_string(XyFirstFlag + 615) + "   ; X/Y's flag 615") != std::string::npos
                  && source.find("push.p.c " + std::to_string(XyFirstWork + 16498 - 0x4000) + "   ; X/Y's work 16498") != std::string::npos && source.find("push.p.c 9\n") != std::string::npos,
              "the source moves them, the value set left");
        bool assembles = true;
        try { AmxAssemble(source); } catch (const FormatError&) { assembles = false; }
        check(assembles && notes.empty(), "the source assembles, nothing computed left");
        // a flag given as a base plus an index (X/Y's ranges): the base moved, the range kept
        const Bytes r = AmxAssemble("code\nmain:\n    load.p.s.pri -4\n    add.p.c 2962\n    push.pri\n    sysreq.n FlagSet 4\n    halt 0\n");
        const XyNumbers rangeNumbers = XyNumbersFor({r}, {}, {});
        std::vector<std::string> rangeNotes;
        const std::string range = XyScriptSource(r, names, rangeNumbers, rangeNotes);
        check(range.find("add.p.c " + std::to_string(XyFirstFlag + 2962) + "   ; X/Y's flag 2962") != std::string::npos && rangeNotes.empty(),
              "a flag range's base moved by the offset");
        // the scenes a zone starts by itself (GetSceneChangeData's (case, work, value) triples): their works numbered and
        // moved, also one no native names (16600), the cases and values kept
        const Bytes z = AmxAssemble(
            "header 10 10 0x1C main 4096\npublic #63F02D54 scenes\n"
            "code\n"
            "main:\n    push.p.c 1\n    push.p.c 16514\n    sysreq.n WorkSet 8\n    halt 0\n"
            "scenes:\n    proc\n    const.p.alt 0\n    load.p.s.pri 12\n    lidx.p.b 2\n    retn\n"
            "data\n    cell 1 16514 0 7 16600 2 -1\n");
        const XyNumbers sceneNumbers = XyNumbersFor({z}, {}, {});
        check(sceneNumbers.Works.size() == 2 && sceneNumbers.Works.count(16600), "a scene's work numbered though no native names it");
        std::vector<std::string> sceneNotes;
        const std::string scenes = XyScriptSource(z, names, sceneNumbers, sceneNotes);
        const std::string cells = "cell 1 " + std::to_string(sceneNumbers.Works.at(16514)) + " 0 7 " + std::to_string(sceneNumbers.Works.at(16600)) + " 2 -1";
        check(scenes.find(cells) != std::string::npos, "the scene table's works moved, its cases and values kept");
        bool scenesAssemble = true;
        try { AmxAssemble(scenes); } catch (const FormatError&) { scenesAssemble = false; }
        check(scenesAssemble && sceneNotes.size() == 1 && sceneNotes[0] == "scene changes: 2 work(s) moved", "it assembles, the move noted");
        std::filesystem::remove(names);
    }
    {
        // XyMoveZones: a zone a procedure passes on to _MapChangeCore (X/Y's wrappers) moved at each call
        const std::string source = "native _MapChangeCore\ncode\n"
            "main:\n    push.p.c 3\n    push.p.c 9\n    push.p.c 0\n    push.p.c 5\n    push.p.c 2\n    push.p.c 20\n    call wrap\n    halt 0\n"
            "wrap:\n    proc\n    push.p.c 0\n    push.p.s 24\n    push.p.s 20\n    push.p.s 16\n    push.p.s 12\n    sysreq.n 0 20\n    retn\n";
        std::vector<std::string> notes;
        const std::string moved = XyMoveZones(source, {{2, {540, 0, 0}}}, notes);
        check(moved.find("push.p.c 540   ; X/Y's zone 2, passed on to a map change") != std::string::npos && moved.find("push.p.c 9\n") != std::string::npos,
              "a zone passed by a procedure moved at its call, the rest kept");
        check(notes.size() == 1 && notes[0].find("1 zone(s)") != std::string::npos, "the move noted");
    }
    {
        // XyMoveMessages: MsgLoad(table, n)'s n moved by (table != 0, n), a computed one noted; XyMessageMember: X's switch
        const std::string source = "native MsgLoad\ncode\n"
            "    push.p.c 21\n    push.p.c 0\n    sysreq.n 0 8\n"
            "    push.p.c 0\n    push.p.c 1\n    sysreq.n 0 8\n"
            "    push.p.s 12\n    push.p.c 1\n    sysreq.n 0 8\n";
        std::set<std::pair<bool, int>> found;
        std::vector<std::string> notes;
        const std::string moved = XyMoveMessages(source, {{{false, 21}, 0x1AF}, {{true, 0}, 0x380}}, found, notes);
        check(moved.find("push.p.c 431   ; X/Y's system text file 21") != std::string::npos
                  && moved.find("push.p.c 896   ; X/Y's story text file 0") != std::string::npos,
              "MsgLoad's file moved by its table and n");
        check(found.size() == 2 && notes.size() == 1 && notes[0].find("computed") != std::string::npos, "a computed file noted, kept");
        check(XyMessageMember(21) == 0x4F && XyMessageMember(0) == 0x8E && XyMessageMember(22) == -1, "X's MsgLoad switch");
    }
    {
        // XyMoveZones: a map change's zone moved and its tile shifted, an unknown zone kept and noted
        const std::string source = "native FlagGet\nnative _MapChangeCore\ncode\n"
            "    push.p.c 0\n    push.p.c 596\n    push.p.c 145\n    push.p.c 467\n    push.p.c 0\n    sysreq.n 1 20\n"
            "    push.p.c 9\n    push.p.c 1\n    push.p.c 1\n    push.p.c 1\n    push.p.c 77\n    sysreq.n 1 20\n";
        std::vector<std::string> notes;
        const std::string moved = XyMoveZones(source, {{0, {538, -440, -560}}}, notes);
        check(moved.find("push.p.c 538   ; X/Y's zone 0") != std::string::npos && moved.find("push.p.c 27   ; X/Y's x 467") != std::string::npos
                  && moved.find("push.p.c 36   ; X/Y's z 596") != std::string::npos && moved.find("push.p.c 145\n") != std::string::npos,
              "a map change's zone moved, its tile shifted, its height kept");
        check(moved.find("push.p.c 77\n") != std::string::npos && notes.size() == 1 && notes[0].find("zone 77 is not in this mod") != std::string::npos,
              "a zone not in the mod kept and noted");
    }
    {
        // XyMoveZones: the zone PlayerGetZoneID's result is compared with, moved; one not in the mod kept and noted
        const std::string source = "native PlayerGetZoneID\ncode\n"
            "    sysreq.n 0 0\n    eq.p.c.pri 0\n    jnz L000010\n    sysreq.n 0 0\n    eq.p.c.pri 77\n    jnz L000010\nL000010:\n";
        std::vector<std::string> notes;
        const std::string moved = XyMoveZones(source, {{0, {538, 0, 0}}}, notes);
        check(moved.find("eq.p.c.pri 538   ; X/Y's zone 0") != std::string::npos, "a zone compared with PlayerGetZoneID's result moved");
        check(moved.find("eq.p.c.pri 77\n") != std::string::npos && notes.size() == 1 && notes[0].find("zone 77") != std::string::npos,
              "one not in the mod kept and noted");
    }
    {
        // XyMoveSounds: an effect's id made Omega Ruby's, a jingle (a stream) moved only with X/Y's music, an unknown id kept
        XySounds sounds;
        sounds.Ids = {{0xA0008, 0xA0010}, {0x50000, 0xB5}};
        const std::string source = "native SEPlay\nnative MEPlay\ncode\n"
            "    push.c 655368\n    sysreq.n 0 4\n    push.c 327680\n    sysreq.n 1 4\n    push.c 655369\n    sysreq.n 0 4\n";
        std::vector<std::string> notes;
        const std::string without = XyMoveSounds(source, sounds, false, notes), with = XyMoveSounds(source, sounds, true, notes);
        check(without.find("push.c 655376   ; X/Y's sound 0xA0008") != std::string::npos && without.find("push.c 327680\n") != std::string::npos,
              "an effect moved, a jingle kept without X/Y's music");
        check(with.find("push.c 181   ; X/Y's sound 0x50000") != std::string::npos && with.find("push.c 655369\n") != std::string::npos,
              "the jingle moved with it, an id of no X/Y sound kept");
    }
    {
        // XyMoveTrainers: a battle's trainer (_CallTrainerBattleCore's second argument) given through the script's own
        // procedure, its first parameter, as X/Y zone 21's Korrina: the caller's constant moved; the procedure's 0 kept
        const std::string source = "native _CallTrainerBattleCore\ncode\n"
            "L000010:\n    proc\n    push.p.s 16\n    push.p.s 12\n    push.p.c 0\n    sysreq.n 0 12\n    retn\n"
            "L000040:\n    proc\n    push.p.c 0\n    push.p.c 613\n    push.p.c 8\n    call L000010\n    retn\n";
        std::vector<std::string> notes;
        const std::string moved = XyMoveTrainers(source, notes);
        check(moved.find("push.p.c 1563   ; X/Y's trainer 613") != std::string::npos && moved.find("    push.p.c 0\n    sysreq.n") != std::string::npos,
              "a battle's trainer moved through the procedure that passes it");
        // a trainer character's flag in X/Y (0x6CC + n) is the one oras-engine --xy-trainers gives trainer 950 + n
        check(XyTrainerToOras(613) == 1563 && XyTrainerToOras(0) == 0, "X/Y's trainer n is Omega Ruby's 950 + n, 0 none");
    }
    Bytes truncated = b;
    truncated.push_back(0); Put32(truncated, 0, (uint32_t)truncated.size());
    bool threw = false;
    try { AmxCells(truncated); } catch (const FormatError&) { threw = true; }
    check(threw, "packed bytes not ending at the script's size refused");
    printf(ok ? "all passed\n" : "FAILED\n");
    return ok ? 0 : 1;
}
