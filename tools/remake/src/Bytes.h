#ifndef REMAKE_BYTES_H
#define REMAKE_BYTES_H

// Little-endian reads over a byte buffer, bounds-checked: game data from a
// cartridge is untrusted, a bad offset throws instead of reading past the end.

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace remake
{

using Bytes = std::vector<uint8_t>;

struct FormatError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

uint8_t U8(const Bytes& b, size_t at);
uint16_t U16(const Bytes& b, size_t at);
uint32_t U32(const Bytes& b, size_t at);
// size bytes from at, checked
Bytes Slice(const Bytes& b, size_t at, size_t size);
// count characters from at, stopping at a zero byte
std::string Text(const Bytes& b, size_t at, size_t count);
Bytes ReadFile(const std::string& path);
void WriteFile(const std::string& path, const Bytes& data);

}

#endif // REMAKE_BYTES_H
