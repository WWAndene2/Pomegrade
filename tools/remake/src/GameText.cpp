#include "GameText.h"

#include <algorithm>
#include <cstdio>

namespace remake
{

static const uint16_t KeyBase = 0x7C89, KeyStep = 0x2983;

static uint16_t NextKey(uint16_t key) { return (uint16_t)((key << 3) | (key >> 13)); }

static void PutUtf8(std::string& out, uint32_t c)
{
    if (c < 0x80) out += (char)c;
    else if (c < 0x800) { out += (char)(0xC0 | c >> 6); out += (char)(0x80 | (c & 0x3F)); }
    else { out += (char)(0xE0 | c >> 12); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
}

static std::string Escape(const std::vector<uint16_t>& u)
{
    std::string out;
    char buf[16];
    for (size_t i = 0; i < u.size(); i++)
    {
        const uint16_t c = u[i];
        // a variable: 0x10, the count of units that follow (the code and its arguments), the code, the arguments
        if (c == 0x10 && i + 2 < u.size() && u[i + 1] >= 1 && i + 1 + u[i + 1] < u.size())
        {
            const uint16_t count = u[i + 1];
            snprintf(buf, sizeof buf, "[VAR %04X(", u[i + 2]);
            out += buf;
            for (uint16_t k = 1; k < count; k++) { snprintf(buf, sizeof buf, k > 1 ? ",%04X" : "%04X", u[i + 2 + k]); out += buf; }
            out += ")]";
            i += 1 + count;
            continue;
        }
        if (c == '\n') out += "\\n";
        else if (c == '\\') out += "\\\\";
        else if (c == '[') out += "\\[";
        else if (c < 0x20 || c >= 0xD800) { snprintf(buf, sizeof buf, "\\x%04X", c); out += buf; } // controls, surrogates, private-use symbols
        else PutUtf8(out, c);
    }
    return out;
}

static std::vector<uint16_t> Unescape(const std::string& s)
{
    std::vector<uint16_t> u;
    for (size_t i = 0; i < s.size();)
    {
        const unsigned char c = (unsigned char)s[i];
        if (c == '\\' && i + 1 < s.size())
        {
            const char e = s[i + 1];
            if (e == 'n') { u.push_back('\n'); i += 2; }
            else if (e == 'x' && i + 6 <= s.size()) { u.push_back((uint16_t)std::stoul(s.substr(i + 2, 4), nullptr, 16)); i += 6; }
            else { u.push_back((uint16_t)e); i += 2; }
        }
        else if (s.compare(i, 5, "[VAR ") == 0)
        {
            const size_t open = s.find('(', i), close = s.find(")]", i);
            if (open == std::string::npos || close == std::string::npos) throw FormatError("game text: unclosed [VAR in " + s);
            std::vector<uint16_t> args;
            for (size_t at = open + 1; at < close;)
            {
                const size_t comma = std::min(s.find(',', at), close);
                args.push_back((uint16_t)std::stoul(s.substr(at, comma - at), nullptr, 16));
                at = comma + 1;
            }
            u.push_back(0x10);
            u.push_back((uint16_t)(args.size() + 1));
            u.push_back((uint16_t)std::stoul(s.substr(i + 5, open - i - 5), nullptr, 16));
            u.insert(u.end(), args.begin(), args.end());
            i = close + 2;
        }
        else if (c < 0x80) { u.push_back(c); i++; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) { u.push_back((uint16_t)(((c & 0x1F) << 6) | (s[i + 1] & 0x3F))); i += 2; }
        else if (i + 2 < s.size()) { u.push_back((uint16_t)(((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F))); i += 3; }
        else throw FormatError("game text: bad UTF-8 in " + s);
    }
    return u;
}

std::vector<std::string> ReadGameText(const Bytes& f)
{
    const int sections = U16(f, 0), count = U16(f, 2);
    const uint32_t sectionOffset = U32(f, 12);
    if (sections != 1) throw FormatError("game text: " + std::to_string(sections) + " sections (1 expected)");
    std::vector<std::string> lines;
    for (int i = 0; i < count; i++)
    {
        const size_t entry = sectionOffset + 4 + 8 * i;
        const uint32_t offset = U32(f, entry);
        const uint16_t length = U16(f, entry + 4);
        uint16_t key = (uint16_t)(KeyBase + KeyStep * i);
        std::vector<uint16_t> u;
        for (uint16_t k = 0; k < length; k++)
        {
            u.push_back(U16(f, sectionOffset + offset + 2 * k) ^ key);
            key = NextKey(key);
        }
        while (!u.empty() && u.back() == 0) u.pop_back(); // the terminator, and the zero padding some files keep (a/0/7/1)
        lines.push_back(Escape(u));
    }
    return lines;
}

Bytes WriteGameText(const std::vector<std::string>& lines)
{
    if (lines.size() > 0xFFFF) throw FormatError("game text: more than 65535 lines");
    std::vector<std::vector<uint16_t>> units;
    for (const std::string& s : lines) { units.push_back(Unescape(s)); units.back().push_back(0); if (units.back().size() > 0xFFFF) throw FormatError("game text: a line over 65535 units"); }
    Bytes table, text;
    const size_t tableSize = 4 + 8 * lines.size();
    for (size_t i = 0; i < units.size(); i++)
    {
        const uint32_t offset = (uint32_t)(tableSize + text.size());
        for (int k = 0; k < 4; k++) table.push_back((uint8_t)(offset >> (8 * k)));
        table.push_back((uint8_t)units[i].size()); table.push_back((uint8_t)(units[i].size() >> 8));
        table.push_back(0); table.push_back(0);
        uint16_t key = (uint16_t)(KeyBase + KeyStep * i);
        for (uint16_t c : units[i]) { const uint16_t e = c ^ key; text.push_back((uint8_t)e); text.push_back((uint8_t)(e >> 8)); key = NextKey(key); }
        while (text.size() % 4) text.push_back(0);
    }
    const uint32_t sectionLength = (uint32_t)(tableSize + text.size());
    Bytes out = {1, 0, (uint8_t)lines.size(), (uint8_t)(lines.size() >> 8)};
    for (uint32_t v : {sectionLength, 0u, 0x10u}) for (int k = 0; k < 4; k++) out.push_back((uint8_t)(v >> (8 * k)));
    for (int k = 0; k < 4; k++) out.push_back((uint8_t)(sectionLength >> (8 * k)));
    out.insert(out.end(), table.begin(), table.end());
    out.insert(out.end(), text.begin(), text.end());
    return out;
}

}
