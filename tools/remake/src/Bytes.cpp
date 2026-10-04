#include "Bytes.h"

#include <fstream>
#include <iterator>

namespace remake
{

static void Need(const Bytes& b, size_t at, size_t size)
{
    if (at > b.size() || size > b.size() - at)
        throw FormatError("read past the end (" + std::to_string(at) + "+" + std::to_string(size) + " of " + std::to_string(b.size()) + ")");
}

uint8_t U8(const Bytes& b, size_t at) { Need(b, at, 1); return b[at]; }
uint16_t U16(const Bytes& b, size_t at) { Need(b, at, 2); return (uint16_t)(b[at] | b[at + 1] << 8); }
uint32_t U32(const Bytes& b, size_t at)
{
    Need(b, at, 4);
    return (uint32_t)b[at] | (uint32_t)b[at + 1] << 8 | (uint32_t)b[at + 2] << 16 | (uint32_t)b[at + 3] << 24;
}

Bytes Slice(const Bytes& b, size_t at, size_t size)
{
    Need(b, at, size);
    return Bytes(b.begin() + at, b.begin() + at + size);
}

std::string Text(const Bytes& b, size_t at, size_t count)
{
    Need(b, at, count);
    std::string s;
    for (size_t i = 0; i < count && b[at + i]; i++) s += (char)b[at + i];
    return s;
}

Bytes ReadFile(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open " + path);
    return Bytes(std::istreambuf_iterator<char>(f), {});
}

void WriteFile(const std::string& path, const Bytes& data)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write " + path);
    f.write((const char*)data.data(), (std::streamsize)data.size());
}

}
