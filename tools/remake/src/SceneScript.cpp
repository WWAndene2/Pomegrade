#include "SceneScript.h"

#include "Amx.h"
#include "Bytes.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

namespace remake
{

namespace
{

std::vector<std::string> SplitLines(const std::string& text)
{
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
    }
    return out;
}

std::vector<std::string> Words(const std::string& line)
{
    std::vector<std::string> out;
    std::istringstream in(line);
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

std::string Trim(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t"), b = s.find_last_not_of(" \t");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// The library script's source: its lines, its natives by hash (their sysreq index), its routines by their code
struct Library
{
    std::vector<std::string> Lines;
    std::vector<uint32_t> Natives;   // in declaration order: a native's index is its sysreq.n operand
    size_t LastNativeLine = 0;       // where a native the scene needs is declared, after the library's own

    explicit Library(const std::string& source) : Lines(SplitLines(source))
    {
        for (size_t i = 0; i < Lines.size(); i++)
        {
            const auto w = Words(Lines[i]);
            if (w.size() < 2 || w[0] != "native") continue;
            Natives.push_back(w[1][0] == '#' ? (uint32_t)std::stoul(w[1].substr(1), nullptr, 16) : AmxNameHash(w[1]));
            LastNativeLine = i;
        }
        if (Natives.empty()) throw FormatError("scene: the library script declares no natives");
    }

    // the sysreq index of a native, declared after the library's own when it has none (AmxAssemble checks the name)
    int Native(const std::string& name)
    {
        const uint32_t hash = AmxNameHash(name);
        for (size_t k = 0; k < Natives.size(); k++)
            if (Natives[k] == hash) return (int)k;
        Lines.insert(Lines.begin() + (long)++LastNativeLine, "native " + name);
        Natives.push_back(hash);
        return (int)Natives.size() - 1;
    }

    // the sysreq index of a native given by hash, declared ("native #HASH") after the script's own when it has none
    int NativeHash(uint32_t hash)
    {
        for (size_t k = 0; k < Natives.size(); k++)
            if (Natives[k] == hash) return (int)k;
        char name[16];
        snprintf(name, sizeof name, "#%08X", hash);
        Lines.insert(Lines.begin() + (long)++LastNativeLine, std::string("native ") + name);
        Natives.push_back(hash);
        return (int)Natives.size() - 1;
    }

    // the label of the routine whose first lines after its label match `body` (each "*" any operand; "N:name" a sysreq
    // index that must be native `name`); throws when none or more than one does
    std::string Routine(const std::vector<std::string>& body, const std::string& what)
    {
        std::vector<std::string> found;
        for (size_t i = 0; i + body.size() < Lines.size(); i++)
        {
            const std::string label = Trim(Lines[i]);
            if (label.empty() || label.back() != ':') continue;
            bool same = true;
            for (size_t k = 0; k < body.size() && same; k++)
            {
                // a line's comment left out (X/Y's translated scripts name each native: "sysreq.n 14 4   ; WorkGet")
                const auto want = Words(body[k]), have = Words(Lines[i + 1 + k].substr(0, Lines[i + 1 + k].find(';')));
                same = want.size() == have.size();
                for (size_t j = 0; j < want.size() && same; j++)
                {
                    if (want[j] == "*") continue;
                    const size_t colon = want[j].find(':');
                    if (colon != std::string::npos && want[j][0] == 'N')
                    {
                        const size_t k2 = std::stoul(have[j]);
                        same = k2 < Natives.size() && Natives[k2] == AmxNameHash(want[j].substr(colon + 1));
                    }
                    else same = want[j] == have[j];
                }
            }
            if (same) found.push_back(label.substr(0, label.size() - 1));
        }
        if (found.size() != 1)
            throw FormatError("scene: the library script has " + std::to_string(found.size()) + " routines for " + what + " (one expected)");
        return found[0];
    }
};

// The procedures `roots` (labels of `from`) and every procedure they call, copied for another script `to`: their labels
// prefixed "lib_", their natives declared in `to` (by hash) and renumbered. A procedure is its label followed by "proc", up
// to the next such label or the data. Throws when a copied line names anything else of `from` (a data label, a public
// variable): the routines a scene uses are code only (Littleroot's, 20 October)
std::vector<std::string> ImportProcedures(const Library& from, Library& to, const std::vector<std::string>& roots)
{
    std::map<std::string, std::pair<size_t, size_t>> procs; // label -> [first line, end)
    std::string open;
    size_t start = 0;
    for (size_t i = 0; i < from.Lines.size(); i++)
    {
        const std::string t = Trim(from.Lines[i].substr(0, from.Lines[i].find(';')));
        const bool begins = !t.empty() && t.back() == ':' && i + 1 < from.Lines.size() && Trim(from.Lines[i + 1]) == "proc";
        if (begins || t == "data")
        {
            if (!open.empty()) procs[open] = {start, i};
            open = begins ? t.substr(0, t.size() - 1) : "";
            start = i;
            if (t == "data") break;
        }
    }
    std::vector<std::string> todo = roots, order;
    std::map<std::string, bool> taken;
    while (!todo.empty())
    {
        const std::string p = todo.back();
        todo.pop_back();
        if (taken[p]) continue;
        const auto at = procs.find(p);
        if (at == procs.end()) throw FormatError("scene: the library has no procedure " + p);
        taken[p] = true;
        order.push_back(p);
        for (size_t i = at->second.first; i < at->second.second; i++)
        {
            const auto w = Words(from.Lines[i].substr(0, from.Lines[i].find(';')));
            if (w.size() == 2 && w[0] == "call") todo.push_back(w[1]);
        }
    }
    // the labels inside the copied procedures, renamed
    std::map<std::string, std::string> rename;
    for (const std::string& p : order)
        for (size_t i = procs[p].first; i < procs[p].second; i++)
        {
            const std::string t = Trim(from.Lines[i].substr(0, from.Lines[i].find(';')));
            if (!t.empty() && t.back() == ':') rename[t.substr(0, t.size() - 1)] = "lib_" + t.substr(0, t.size() - 1);
        }
    std::vector<std::string> out;
    for (const std::string& p : order)
        for (size_t i = procs[p].first; i < procs[p].second; i++)
        {
            const std::string t = Trim(from.Lines[i].substr(0, from.Lines[i].find(';')));
            if (t.empty()) continue;
            if (t.back() == ':') { out.push_back(rename.at(t.substr(0, t.size() - 1)) + ":"); continue; }
            auto w = Words(t);
            if (w[0] == "sysreq.n" && w.size() == 3)
            {
                const size_t k = std::stoul(w[1]);
                if (k >= from.Natives.size()) throw FormatError("scene: the library calls native " + w[1] + ", not declared");
                w[1] = std::to_string(to.NativeHash(from.Natives[k]));
            }
            else
                for (size_t j = 1; j < w.size(); j++)
                {
                    if (rename.count(w[j])) { w[j] = rename.at(w[j]); continue; }
                    const char c = w[j][0];
                    if (!(std::isdigit((unsigned char)c) || c == '-'))
                        throw FormatError("scene: the library's procedure " + p + " names " + w[j] + ", which is not code it copies");
                }
            std::string line = "   ";
            for (const auto& x : w) line += " " + x;
            out.push_back(line);
        }
    return out;
}

// work 0x8010 after the yes/no box: the first choice (yes); measured headless
constexpr int YesAnswer = 0;

// a float as the cell the game reads it from (the camera natives take floats)
std::string FloatCell(double v)
{
    const float f = (float)v;
    int32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    return std::to_string(bits);
}

double Real(const std::string& word, int line)
{
    try
    {
        size_t used = 0;
        const double v = std::stod(word, &used);
        if (used == word.size()) return v;
    }
    catch (const std::exception&) {}
    throw FormatError("scene line " + std::to_string(line) + ": \"" + word + "\" is not a number");
}

int Number(const std::string& word, int line)
{
    try
    {
        size_t used = 0;
        const long v = std::stol(word, &used, 0);
        if (used == word.size()) return (int)v;
    }
    catch (const std::exception&) {}
    throw FormatError("scene line " + std::to_string(line) + ": \"" + word + "\" is not a number");
}

}

std::string CompileScene(const std::string& scene, const std::string& librarySource, int lineBase, bool chain, const std::string& targetSource)
{
    Library lib(librarySource);

    // the routines the statements call, found by their code (Littleroot's: say L000348, wait L000004, 16 October). X/Y's
    // talking routine is the same without the WorkSet(0x8016, 1) before turning the speaker (Vaniville's L0008E8, X/Y
    // zone 0 translated, 20 October): either is taken
    std::string say;
    try
    {
        say = lib.Routine({"proc", "load.p.s.pri 20", "eq.p.c.pri -1", "jzer *", "push.c 32785", "sysreq.n N:WorkGet 4",
                           "stor.p.s.pri 20", "*", "push.p.c 1", "push.c 32790", "sysreq.n N:WorkSet 8", "push.p.s 20",
                           "push.p.c 4", "call *"},
                          "talking (say)");
    }
    catch (const FormatError&)
    {
        say = lib.Routine({"proc", "load.p.s.pri 20", "eq.p.c.pri -1", "jzer *", "push.c 32785", "sysreq.n N:WorkGet 4",
                           "stor.p.s.pri 20", "*", "push.p.s 20", "push.p.c 4", "call *", "push.p.c 0", "push.p.s 16"},
                          "talking (say, Omega Ruby's or X/Y's shape)");
    }
    std::string wait = lib.Routine({"proc", "push.p.s 12", "sysreq.n N:_Suspend 4", "zero.pri", "zero.alt", "halt 12"},
                                         "waiting (wait)");
    // waiting for a person's commands to end: MdlAcmdUpdate each frame until it reports them done
    std::string actWait = lib.Routine({"proc", "jump *", "*", "push.p.s 12", "sysreq.n N:MdlAcmdUpdate 4", "jzer *"},
                                            "waiting for a person's commands (act)");
    // fading: the screen from the field (FadeRequestOut, called by the scripts themselves), back to it (the routine that
    // waits for the map's blocks, at most 60 frames, then _FadeRequestIn), and the wait for either's end (FadeIsEnd)
    std::string fadeIn = lib.Routine({"proc", "push.p.c 0", "zero.p.s -4", "jump *", "*", "inc.p.s -4", "*",
                                            "load.p.s.pri -4", "const.p.alt 60", "jsgeq *", "sysreq.n N:_FieldMapBlockIsLoading 0"},
                                           "fading in (fade in)");
    std::string fadeWait = lib.Routine({"proc", "jump *", "*", "sysreq.n N:FadeIsEnd 0", "jzer *"}, "waiting for a fade (fade)");
    // into another script (targetSource): the routines copied there with every procedure they call, the scene's code then
    // written for that script (its natives, its g_mode, its main to chain to)
    std::unique_ptr<Library> target;
    std::vector<std::string> imported;
    if (!targetSource.empty())
    {
        target = std::make_unique<Library>(targetSource);
        imported = ImportProcedures(lib, *target, {say, wait, actWait, fadeIn, fadeWait});
        for (std::string* r : {&say, &wait, &actWait, &fadeIn, &fadeWait}) *r = "lib_" + *r;
    }
    Library& out = target ? *target : lib;
    // g_mode, the event's number: the public variable every zone script declares (#D7477C97)
    std::string mode;
    for (const auto& l : out.Lines)
    {
        const auto w = Words(l);
        if (w.size() == 3 && w[0] == "pubvar" && (w[1] == "g_mode" || w[1] == "#D7477C97")) mode = w[2];
    }
    if (mode.empty()) throw FormatError("scene: the library script has no g_mode");

    // the events, compiled; each label prefixed "scene_" (the library's are L + its address)
    std::vector<std::string> code;
    std::vector<std::pair<int, std::string>> events;
    std::vector<std::pair<std::string, bool>> ifs;   // open ifs: their else label, whether "else" was given
    int labels = 0, lineNo = 0;
    const auto label = [&]() { return "scene_" + std::to_string(labels++); };
    const auto close = [&]() {
        if (!ifs.empty()) throw FormatError("scene: event " + std::to_string(events.back().first) + " has an \"if\" without its \"endif\"");
        if (!events.empty()) code.push_back("    jump scene_end");
    };
    for (const std::string& raw : SplitLines(scene))
    {
        lineNo++;
        const std::string line = Trim(raw.substr(0, raw.find('#')));
        if (line.empty()) continue;
        const auto w = Words(line);
        const auto need = [&](size_t n) {
            if (w.size() != n) throw FormatError("scene line " + std::to_string(lineNo) + ": \"" + line + "\" takes " + std::to_string(n - 1) + " value(s)");
        };
        if (w[0] == "on")
        {
            need(2);
            close();
            const int n = Number(w[1], lineNo);
            for (const auto& e : events)
                if (e.first == n) throw FormatError("scene line " + std::to_string(lineNo) + ": event " + w[1] + " given twice");
            events.push_back({n, label()});
            code.push_back(events.back().second + ":");
            continue;
        }
        if (events.empty()) throw FormatError("scene line " + std::to_string(lineNo) + ": \"" + line + "\" before any \"on\"");
        if (w[0] == "say")
        {
            // say L [by N]: the speaker the person talked to (-1), or character N of the zone (a scene started by a trigger
            // has no one talked to)
            if (w.size() != 2 && !(w.size() == 4 && w[2] == "by")) throw FormatError("scene line " + std::to_string(lineNo) + ": \"say L [by N]\"");
            const int by = w.size() == 4 ? Number(w[3], lineNo) : -1;
            code.insert(code.end(), {"    push.c " + std::to_string(by), "    push.p.c 1", "    push.c " + std::to_string(lineBase + Number(w[1], lineNo)),
                                     "    push.p.c 12", "    call " + say});
        }
        else if (w[0] == "ask")
        {
            // ask L [by N]: line L said (as "say"), then the yes/no box (YesNoWin_Seq 0 0 0 0, as the game's 89 scripts
            // give it, polled each frame until it returns non-zero); the answer stays in work 0x8010 for "if yes" / "if no"
            if (w.size() != 2 && !(w.size() == 4 && w[2] == "by")) throw FormatError("scene line " + std::to_string(lineNo) + ": \"ask L [by N]\"");
            const int by = w.size() == 4 ? Number(w[3], lineNo) : -1;
            const std::string again = label(), answered = label();
            code.insert(code.end(), {"    push.c " + std::to_string(by), "    push.p.c 1", "    push.c " + std::to_string(lineBase + Number(w[1], lineNo)),
                                     "    push.p.c 12", "    call " + say,
                                     again + ":", "    push.p.c 0", "    push.p.c 0", "    push.p.c 0", "    push.p.c 0",
                                     "    sysreq.n " + std::to_string(out.Native("YesNoWin_Seq")) + " 16", "    jnz " + answered,
                                     "    push.p.c 1", "    push.p.c 4", "    call " + wait, "    jump " + again, answered + ":"});
        }
        else if (w[0] == "wait")
        {
            need(2);
            code.insert(code.end(), {"    push.c " + std::to_string(Number(w[1], lineNo)), "    push.p.c 4", "    call " + wait});
        }
        else if (w[0] == "act" || w[0] == "walk" || w[0] == "face")
        {
            // act WHO CODE DIR [N]: N times the movement command CODE towards DIR (MdlAcmdSet), then waits for its end;
            // walk WHO DIR [N] is code 19, face WHO DIR code 6 (run sceneD1); WHO and DIR may be named
            std::vector<std::string> a = w;
            if (w[0] == "walk" || w[0] == "face")
            {
                if (w.size() != 3 && !(w[0] == "walk" && w.size() == 4))
                    throw FormatError("scene line " + std::to_string(lineNo) + ": \"walk WHO DIR [N]\" or \"face WHO DIR\"");
                a = {"act", w[1], w[0] == "walk" ? "19" : "6", w[2]};
                if (w.size() == 4) a.push_back(w[3]);
            }
            if (a.size() != 4 && a.size() != 5) throw FormatError("scene line " + std::to_string(lineNo) + ": \"act WHO CODE DIR [N]\"");
            static const std::map<std::string, std::string> names = {{"player", "255"}, {"person", "-1"}, {"north", "0"},
                                                                     {"south", "1"}, {"west", "2"}, {"east", "3"}};
            for (size_t k : {size_t(1), size_t(3)})
                if (names.count(a[k])) a[k] = names.at(a[k]);
            const auto& w = a;
            const int who = Number(w[1], lineNo), times = w.size() == 5 ? Number(w[4], lineNo) : 1;
            if (times < 1 || times > 64) throw FormatError("scene line " + std::to_string(lineNo) + ": act repeats 1 to 64 times");
            code.insert(code.end(), {"    push.c " + std::to_string(who), "    sysreq.n " + std::to_string(out.Native("MdlAcmdInit")) + " 4"});
            for (int t = 0; t < times; t++)
                code.insert(code.end(), {"    push.p.c 0", "    push.p.c 1", "    push.c " + std::to_string(Number(w[3], lineNo)),
                                         "    push.c " + std::to_string(Number(w[2], lineNo)),
                                         "    sysreq.n " + std::to_string(out.Native("MdlAcmdSet")) + " 16"});
            code.insert(code.end(), {"    sysreq.n " + std::to_string(out.Native("MdlAcmdSetEnd")) + " 0",
                                     "    push.c " + std::to_string(who), "    push.p.c 4", "    call " + actWait});
        }
        else if (w[0] == "fade")
        {
            // fade out|in KIND [F]: the screen fades from the field, or back to it, over F frames (30 by default), and
            // the scene waits for its end; KIND both (2), top (0), or a number as the game's scripts give it
            if ((w.size() != 3 && w.size() != 4) || (w[1] != "out" && w[1] != "in"))
                throw FormatError("scene line " + std::to_string(lineNo) + ": \"fade out|in both|top [F]\"");
            // kind 2 fades both screens, 0 the top one only (run sceneE1: the bottom screen's brightness 77 -> 18 and 77)
            const int kind = w[2] == "both" ? 2 : w[2] == "top" ? 0 : Number(w[2], lineNo), frames = w.size() == 4 ? Number(w[3], lineNo) : 30;
            if (w[1] == "out")
                code.insert(code.end(), {"    push.c " + std::to_string(frames), "    push.c " + std::to_string(kind), "    push.p.c 0",
                                         "    sysreq.n " + std::to_string(out.Native("FadeRequestOut")) + " 12"});
            else
                code.insert(code.end(), {"    push.c " + std::to_string(frames), "    push.c " + std::to_string(kind), "    push.p.c 8",
                                         "    call " + fadeIn});
            code.insert(code.end(), {"    push.p.c 0", "    call " + fadeWait});
        }
        else if (w[0] == "camera")
        {
            // camera X Z H TX TZ TH [FOV [F]]: the camera moves, over F frames (60 by default), to stand at tile X, Z,
            // H tiles up, looking at tile TX, TZ, TH tiles up (a tile is 18 world units; a tile's centre at +0.5), with a
            // field of view of FOV degrees (30, the field's, by default): EVCameraSetInitParam (the field camera as the
            // start), EVCameraSetDstParam (0, position, target, fov, 0), EVCameraSetFrameParam (0, F, 1, 0),
            // EVCameraBegin, then EVCameraIsEndFrame each frame. "camera back [F]": EVCameraEnd(F), the field's camera
            if (w.size() >= 2 && w[1] == "back")
            {
                if (w.size() > 3) throw FormatError("scene line " + std::to_string(lineNo) + ": \"camera back [F]\"");
                code.insert(code.end(), {"    push.c " + std::to_string(w.size() == 3 ? Number(w[2], lineNo) : 0),
                                         "    sysreq.n " + std::to_string(out.Native("EVCameraEnd")) + " 4"});
                continue;
            }
            if (w.size() < 7 || w.size() > 9) throw FormatError("scene line " + std::to_string(lineNo) + ": \"camera X Z H TX TZ TH [FOV [F]]\"");
            const double unit = 18;
            // pushed last to first: the native reads 0, then x, y, z of the position, x, y, z of the target, fov, roll
            const double params[] = {Real(w[1], lineNo) * unit, Real(w[3], lineNo) * unit, Real(w[2], lineNo) * unit,
                                     Real(w[4], lineNo) * unit, Real(w[6], lineNo) * unit, Real(w[5], lineNo) * unit,
                                     w.size() >= 8 ? Real(w[7], lineNo) : 30.0, 0.0};
            code.push_back("    sysreq.n " + std::to_string(out.Native("EVCameraSetInitParam")) + " 0");
            for (int k = 7; k >= 0; k--) code.push_back("    push.c " + FloatCell(params[k]));
            code.insert(code.end(), {"    push.p.c 0", "    sysreq.n " + std::to_string(out.Native("EVCameraSetDstParam")) + " 36",
                                     "    push.p.c 0", "    push.p.c 1", "    push.c " + std::to_string(w.size() == 9 ? Number(w[8], lineNo) : 60),
                                     "    push.p.c 0", "    sysreq.n " + std::to_string(out.Native("EVCameraSetFrameParam")) + " 16",
                                     "    sysreq.n " + std::to_string(out.Native("EVCameraBegin")) + " 0"});
            const std::string again = label();
            code.insert(code.end(), {again + ":", "    push.p.c 1", "    push.p.c 4", "    call " + wait,
                                     "    sysreq.n " + std::to_string(out.Native("EVCameraIsEndFrame")) + " 0", "    jzer " + again});
        }
        else if (w[0] == "battle")
        {
            // battle T: trainer T of a/0/3/6 (_CallTrainerBattleCore 0, T, 0, 0, 0, 0, 0, as the scripts that start a
            // trainer's battle do), a frame's wait, then the result (_BattleGetResult's first cell): 0 is a loss, which
            // the game's handler takes over (CallTrainerLose) and the event ends there
            need(2);
            const std::string won = label();
            code.insert(code.end(), {"    push.p.c 0", "    push.p.c 0", "    push.p.c 0", "    push.p.c 0", "    push.p.c 0",
                                     "    push.c " + std::to_string(Number(w[1], lineNo)), "    push.p.c 0",
                                     "    sysreq.n " + std::to_string(out.Native("_CallTrainerBattleCore")) + " 28",
                                     "    push.p.c 1", "    push.p.c 4", "    call " + wait,
                                     "    stack.p -32", "    zero.pri", "    addr.alt -32", "    fill 32", "    push.p.adr -32",
                                     "    sysreq.n " + std::to_string(out.Native("_BattleGetResult")) + " 4",
                                     "    addr.p.pri -32", "    load.i", "    stack.p 32", "    jnz " + won,
                                     "    sysreq.n " + std::to_string(out.Native("CallTrainerLose")) + " 0", "    jump scene_end",
                                     won + ":"});
        }
        else if (w[0] == "give")
        {
            // give pokemon N: entry N (0-36) of the game's gift table (PokePartyAdd N 0, as the scripts give Beldum: 21 0;
            // the table, DllField 0x1033B06C: 37 records of 0x24 bytes, species then level): nothing when the party is full
            if (w.size() == 3 && w[1] == "pokemon")
            {
                const int n = Number(w[2], lineNo);
                if (n < 0 || n > 36) throw FormatError("scene line " + std::to_string(lineNo) + ": the gift table has entries 0 to 36");
                code.insert(code.end(), {"    push.p.c 0", "    push.c " + std::to_string(n),
                                         "    sysreq.n " + std::to_string(out.Native("PokePartyAdd")) + " 8"});
                continue;
            }
            // give item I [N]: N of item I (1 by default) into the bag, with no message (ItemAdd I N); a "say" tells it
            if ((w.size() != 3 && w.size() != 4) || w[1] != "item") throw FormatError("scene line " + std::to_string(lineNo) + ": \"give item I [N]\" or \"give pokemon N\"");
            code.insert(code.end(), {"    push.c " + std::to_string(w.size() == 4 ? Number(w[3], lineNo) : 1),
                                     "    push.c " + std::to_string(Number(w[2], lineNo)),
                                     "    sysreq.n " + std::to_string(out.Native("ItemAdd")) + " 8"});
        }
        else if (w[0] == "heal")
        {
            // heal: the party's HP, status and PP restored (PokePartyRecoverAll)
            need(1);
            code.push_back("    sysreq.n " + std::to_string(out.Native("PokePartyRecoverAll")) + " 0");
        }
        else if (w[0] == "flag")
        {
            need(3);
            if (w[2] != "on" && w[2] != "off") throw FormatError("scene line " + std::to_string(lineNo) + ": a flag is set \"on\" or \"off\"");
            const int n = Number(w[1], lineNo);
            // 0xD00-0x2C3F only with oras-engine --story large (save block 47, OrasEngine.cpp)
            if (n < 1 || (n > 0x2C3F && n < 0x3000) || n > 0x303F)
                throw FormatError("scene line " + std::to_string(lineNo) + ": flag " + w[1] + " is outside 1-0x2C3F and 0x3000-0x303F (fatal in the game; 0xD00-0x2C3F need oras-engine --story large)");
            code.insert(code.end(), {"    push.c " + std::to_string(n),
                                     "    sysreq.n " + std::to_string(out.Native(w[2] == "on" ? "FlagSet" : "FlagReset")) + " 4"});
        }
        else if (w[0] == "if")
        {
            if (w.size() == 2 && (w[1] == "yes" || w[1] == "no"))
            {
                // the last "ask"'s answer: work 0x8010, YesAnswer for the first choice (SceneScript.h)
                const std::string otherwise = label();
                code.insert(code.end(), {"    push.c 32784", "    sysreq.n " + std::to_string(out.Native("WorkGet")) + " 4",
                                         "    eq.c.pri " + std::to_string(YesAnswer), w[1] == "yes" ? "    jzer " + otherwise : "    jnz " + otherwise});
                ifs.push_back({otherwise, false});
                continue;
            }
            const bool negated = w.size() == 4 && w[1] == "not";
            const std::string what = w.size() >= 3 ? w[negated ? 2 : 1] : "";
            if (!(w.size() == 3 || negated) || (what != "flag" && what != "item" && what != "party"))
                throw FormatError("scene line " + std::to_string(lineNo) + ": \"if [not] flag N\", \"if [not] item I\" or \"if [not] party N\"");
            const std::string otherwise = label();
            if (what == "party")
            {
                // at least N Pokemon in the party (PokePartyGetCount 0, as the game's 54 calls give it)
                code.insert(code.end(), {"    push.p.c 0", "    sysreq.n " + std::to_string(out.Native("PokePartyGetCount")) + " 4",
                                         "    const.alt " + std::to_string(Number(w.back(), lineNo)),
                                         negated ? "    jsgeq " + otherwise : "    jsless " + otherwise});
                ifs.push_back({otherwise, false});
                continue;
            }
            // a flag set, or at least one of item I in the bag (ItemGetNum)
            code.insert(code.end(), {"    push.c " + std::to_string(Number(w.back(), lineNo)),
                                     "    sysreq.n " + std::to_string(out.Native(what == "flag" ? "FlagGet" : "ItemGetNum")) + " 4",
                                     negated ? "    jnz " + otherwise : "    jzer " + otherwise});
            ifs.push_back({otherwise, false});
        }
        else if (w[0] == "else")
        {
            need(1);
            if (ifs.empty() || ifs.back().second) throw FormatError("scene line " + std::to_string(lineNo) + ": \"else\" without its \"if\"");
            const std::string after = label();
            code.insert(code.end(), {"    jump " + after, ifs.back().first + ":"});
            ifs.back() = {after, true};
        }
        else if (w[0] == "endif")
        {
            need(1);
            if (ifs.empty()) throw FormatError("scene line " + std::to_string(lineNo) + ": \"endif\" without its \"if\"");
            code.push_back(ifs.back().first + ":");
            ifs.pop_back();
        }
        else throw FormatError("scene line " + std::to_string(lineNo) + ": unknown statement \"" + w[0] + "\"");
    }
    close();
    if (events.empty()) throw FormatError("scene: no event (\"on N\")");

    // main: g_mode's event; chained, any other goes to the library's own main (the zone's events kept), else nothing for
    // -1 and CommandNOP for any other (as the game's mains)
    std::string libraryMain;
    for (const auto& l : out.Lines)
    {
        const auto w = Words(l);
        if (w.size() == 6 && w[0] == "header") libraryMain = w[4];
    }
    if (chain && (libraryMain.empty() || (!target && libraryMain[0] != 'L'))) throw FormatError("scene: the library's main is not a code label");
    std::string table = "    casetbl " + std::to_string(events.size() + (chain ? 0 : 1)) + " scene_other" + (chain ? "" : " -1 scene_end");
    for (const auto& e : events) table += " " + std::to_string(e.first) + " " + e.second;
    std::vector<std::string> main = {"scene_main:", "    proc", "    load.pri " + mode, "    switch scene_cases"};
    main.insert(main.end(), code.begin(), code.end());
    if (chain)
        // the library's main is a proc of its own: called as the game calls main (no argument), its result returned
        main.insert(main.end(), {"scene_other:", "    push.p.c 0", "    call " + libraryMain, "    retn"});
    else
        main.insert(main.end(), {"scene_other:", "    sysreq.n " + std::to_string(out.Native("CommandNOP")) + " 0", "    jump scene_end"});
    main.insert(main.end(), {"scene_cases:", table, "scene_end:", "    zero.pri", "    retn"});

    // into the library: before its data, and the header's cip moved to it
    std::ostringstream text;
    bool placed = false, header = false;
    for (const auto& l : out.Lines)
    {
        auto w = Words(l);
        if (!w.empty() && w[0] == "header" && w.size() == 6)
        {
            w[4] = "scene_main";
            text << w[0] << ' ' << w[1] << ' ' << w[2] << ' ' << w[3] << ' ' << w[4] << ' ' << w[5] << '\n';
            header = true;
            continue;
        }
        if (Trim(l) == "data" && !placed)
        {
            for (const auto& m : imported) text << m << '\n';
            for (const auto& m : main) text << m << '\n';
            placed = true;
        }
        text << l << '\n';
    }
    if (!placed || !header) throw FormatError("scene: the library script has no header or no data section");
    return text.str();
}

}
