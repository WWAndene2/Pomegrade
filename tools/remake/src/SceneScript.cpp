#include "SceneScript.h"

#include "Amx.h"
#include "Bytes.h"

#include <map>
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
                const auto want = Words(body[k]), have = Words(Lines[i + 1 + k]);
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

std::string CompileScene(const std::string& scene, const std::string& librarySource)
{
    Library lib(librarySource);

    // the routines the statements call, found by their code (Littleroot's: say L000348, wait L000004, 16 October)
    const std::string say = lib.Routine({"proc", "load.p.s.pri 20", "eq.p.c.pri -1", "jzer *", "push.c 32785",
                                         "sysreq.n N:WorkGet 4", "stor.p.s.pri 20", "*", "push.p.c 1", "push.c 32790",
                                         "sysreq.n N:WorkSet 8", "push.p.s 20", "push.p.c 4", "call *"},
                                        "talking (say)");
    const std::string wait = lib.Routine({"proc", "push.p.s 12", "sysreq.n N:_Suspend 4", "zero.pri", "zero.alt", "halt 12"},
                                         "waiting (wait)");
    // waiting for a person's commands to end: MdlAcmdUpdate each frame until it reports them done
    const std::string actWait = lib.Routine({"proc", "jump *", "*", "push.p.s 12", "sysreq.n N:MdlAcmdUpdate 4", "jzer *"},
                                            "waiting for a person's commands (act)");
    // g_mode, the event's number: the public variable every zone script declares (#D7477C97)
    std::string mode;
    for (const auto& l : lib.Lines)
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
            need(2);
            code.insert(code.end(), {"    push.p.c -1", "    push.p.c 1", "    push.c " + std::to_string(Number(w[1], lineNo)),
                                     "    push.p.c 12", "    call " + say});
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
            code.insert(code.end(), {"    push.c " + std::to_string(who), "    sysreq.n " + std::to_string(lib.Native("MdlAcmdInit")) + " 4"});
            for (int t = 0; t < times; t++)
                code.insert(code.end(), {"    push.p.c 0", "    push.p.c 1", "    push.c " + std::to_string(Number(w[3], lineNo)),
                                         "    push.c " + std::to_string(Number(w[2], lineNo)),
                                         "    sysreq.n " + std::to_string(lib.Native("MdlAcmdSet")) + " 16"});
            code.insert(code.end(), {"    sysreq.n " + std::to_string(lib.Native("MdlAcmdSetEnd")) + " 0",
                                     "    push.c " + std::to_string(who), "    push.p.c 4", "    call " + actWait});
        }
        else if (w[0] == "flag")
        {
            need(3);
            if (w[2] != "on" && w[2] != "off") throw FormatError("scene line " + std::to_string(lineNo) + ": a flag is set \"on\" or \"off\"");
            const int n = Number(w[1], lineNo);
            if (n < 1 || (n > 0xCFF && n < 0x3000) || n > 0x303F)
                throw FormatError("scene line " + std::to_string(lineNo) + ": flag " + w[1] + " is outside 1-0xCFF and 0x3000-0x303F (fatal in the game)");
            code.insert(code.end(), {"    push.c " + std::to_string(n),
                                     "    sysreq.n " + std::to_string(lib.Native(w[2] == "on" ? "FlagSet" : "FlagReset")) + " 4"});
        }
        else if (w[0] == "if")
        {
            const bool negated = w.size() == 4 && w[1] == "not";
            if (!(w.size() == 3 || negated) || w[negated ? 2 : 1] != "flag")
                throw FormatError("scene line " + std::to_string(lineNo) + ": \"if flag N\" or \"if not flag N\"");
            const std::string otherwise = label();
            code.insert(code.end(), {"    push.c " + std::to_string(Number(w.back(), lineNo)),
                                     "    sysreq.n " + std::to_string(lib.Native("FlagGet")) + " 4",
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

    // main: g_mode's event, nothing for -1, CommandNOP for any other (as the game's mains)
    std::string table = "    casetbl " + std::to_string(events.size() + 1) + " scene_other -1 scene_end";
    for (const auto& e : events) table += " " + std::to_string(e.first) + " " + e.second;
    std::vector<std::string> main = {"scene_main:", "    proc", "    load.pri " + mode, "    switch scene_cases"};
    main.insert(main.end(), code.begin(), code.end());
    main.insert(main.end(), {"scene_other:", "    sysreq.n " + std::to_string(lib.Native("CommandNOP")) + " 0", "    jump scene_end",
                             "scene_cases:", table, "scene_end:", "    zero.pri", "    retn"});

    // into the library: before its data, and the header's cip moved to it
    std::ostringstream out;
    bool placed = false, header = false;
    for (const auto& l : lib.Lines)
    {
        auto w = Words(l);
        if (!w.empty() && w[0] == "header" && w.size() == 6)
        {
            w[4] = "scene_main";
            out << w[0] << ' ' << w[1] << ' ' << w[2] << ' ' << w[3] << ' ' << w[4] << ' ' << w[5] << '\n';
            header = true;
            continue;
        }
        if (Trim(l) == "data" && !placed)
        {
            for (const auto& m : main) out << m << '\n';
            placed = true;
        }
        out << l << '\n';
    }
    if (!placed || !header) throw FormatError("scene: the library script has no header or no data section");
    return out.str();
}

}
