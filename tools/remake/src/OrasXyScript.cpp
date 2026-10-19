#include "OrasXyScript.h"

#include "Amx.h"

#include <cstdio>
#include <map>

namespace remake
{

const std::vector<std::pair<std::string, std::string>>& XyNativeRenames()
{
    static const std::vector<std::pair<std::string, std::string>> renames = {
        {"FadeRequestIn", "_FadeRequestIn"},
        {"GetFieldTradeData", "FieldTradeGetData"},
        {"EventBGMEnd", "BGMEndEvent"},
        {"ReqLoadSpMotion", "SpMotionReqLoad"},
        {"IsEndLoadSpMotion", "SpMotionLoadIsEnd"},
        {"SetupSpMotion", "SpMotionSetup"},
        {"PlaySpMotion", "SpMotionPlay"},
        {"DeleteSpMotion", "SpMotionDelete"},
    };
    return renames;
}

XyScriptNatives XyScriptToOras(const Bytes& xScript, const std::string& namesFile, uint32_t mask)
{
    const auto check = AmxNativeCheck(namesFile, mask);
    // Omega Ruby's native names by hash, from the same file (the names AmxNativeCheck knows)
    std::map<uint32_t, std::string> oras;
    {
        const Bytes text = ReadFile(namesFile);
        const std::string all(text.begin(), text.end());
        for (size_t at = all.find("\tScript_Native_"); at != std::string::npos; at = all.find("\tScript_Native_", at + 1))
        {
            const size_t q = all.find("native", at), open = all.find('"', q), close = all.find('"', open + 1);
            if (q == std::string::npos || open == std::string::npos || close == std::string::npos) break;
            const std::string name = all.substr(open + 1, close - open - 1);
            oras[AmxNameHash(name)] = name;
        }
    }
    std::map<uint32_t, std::pair<std::string, std::string>> renamed;
    for (const auto& [x, o] : XyNativeRenames()) renamed[AmxNameHash(x)] = {x, o};

    AmxScript s = AmxScript::Read(xScript);
    XyScriptNatives out;
    for (AmxScript::Entry& native : s.Natives)
    {
        std::string name, xName = "?";
        if (const auto r = renamed.find(native.Name); r != renamed.end())
        {
            xName = r->second.first;
            name = r->second.second;
            native.Name = AmxNameHash(name);
            out.Renamed.push_back(xName + " -> " + name);
        }
        else if (const auto o = oras.find(native.Name); o != oras.end()) xName = name = o->second;
        const std::string why = name.empty() ? "not in Omega Ruby's tables" : check(name);
        if (!why.empty())
        {
            char hash[16];
            snprintf(hash, sizeof hash, "#%08X", native.Name);
            out.Missing.push_back(std::string(hash) + " (" + xName + "): " + why);
        }
    }
    out.Script = s.Write();
    return out;
}

}
