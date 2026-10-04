#include "Inventory.h"

#include "FormatSniffer.h"
#include "Narc.h"
#include "NitroCompression.h"

#include <map>
#include <sstream>

namespace remake
{

static std::string Escape(const std::string& s)
{
    std::string o;
    for (char c : s)
    {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if ((unsigned char)c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o += c;
    }
    return o;
}

std::string DeepKind(const Bytes& data)
{
    const FileKind k = Sniff(data);
    if (k.Id != "lz10" && k.Id != "lz11") return k.Id;
    try { return k.Id + ":" + Sniff(LzDecompress(data)).Id; }
    catch (const FormatError&) { return "unknown"; } // looked compressed, was not
}

std::string InventoryJson(const NdsRom& rom)
{
    std::ostringstream o;
    o << "{\n  \"title\": \"" << Escape(rom.Title()) << "\",\n  \"gameCode\": \"" << Escape(rom.GameCode()) << "\",\n  \"files\": [\n";
    bool first = true;
    for (const NdsFile& f : rom.Files())
    {
        const Bytes data = rom.Read(f);
        o << (first ? "" : ",\n") << "    {\"id\": " << f.Id << ", \"path\": \"" << Escape(f.Path) << "\", \"size\": " << f.Size()
          << ", \"kind\": \"" << DeepKind(data) << "\"";
        first = false;
        if (Narc::Is(data))
        {
            try
            {
                const Narc narc(data);
                std::map<std::string, int> kinds;
                for (size_t i = 0; i < narc.Count(); i++) kinds[DeepKind(narc.Member(i))]++;
                o << ", \"members\": " << narc.Count() << ", \"memberKinds\": {";
                bool k1 = true;
                for (auto& [kind, n] : kinds) { o << (k1 ? "" : ", ") << "\"" << kind << "\": " << n; k1 = false; }
                o << "}";
            }
            catch (const FormatError& e) { o << ", \"error\": \"" << Escape(e.what()) << "\""; }
        }
        o << "}";
    }
    o << "\n  ]\n}\n";
    return o.str();
}

}
