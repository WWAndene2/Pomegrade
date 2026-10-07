#include "OrasMemory.h"
#include "CodePatch.h"

#include <filesystem>

namespace remake
{

// the ARM11 local system caps start at 0x200 of the extended header (after the 0x200-byte system control info); their flags0
// byte (ideal processor, affinity mask, system mode) is at 0x0E of them
static constexpr size_t LocalCaps = 0x200, Flags0 = LocalCaps + 0x0E;

Bytes ExHeaderWithSystemMode(const Bytes& exheader, OrasMemoryMode mode)
{
    if (exheader.size() != 0x800) throw FormatError("the extended header is not 0x800 bytes");
    // a decrypted header names its program in the system info's jump id (0x1C8, after its savedata size) and the local caps
    // (0x200): Azahar warns
    // when they differ, as for an encrypted one
    if (!std::equal(exheader.begin() + 0x1C8, exheader.begin() + 0x1D0, exheader.begin() + LocalCaps))
        throw FormatError("the extended header is not decrypted (its jump id and program id differ)");
    Bytes out = exheader;
    out[Flags0] = (uint8_t)((out[Flags0] & 0x0F) | ((unsigned)mode << 4));
    return out;
}

std::vector<std::string> BuildMemoryMod(N3dsRom& oras, OrasMemoryMode mode, uint32_t linearHeap, const std::string& outDir)
{
    static const struct { OrasMemoryMode Mode; uint32_t Bytes; } modes[] = {
        {OrasMemoryMode::Prod64, 64u << 20}, {OrasMemoryMode::Dev1_96, 96u << 20}, {OrasMemoryMode::Dev2_80, 80u << 20}, {OrasMemoryMode::Dev3_72, 72u << 20}};
    uint32_t total = 0;
    for (const auto& m : modes) if (m.Mode == mode) total = m.Bytes;
    if (!total) throw FormatError("unknown memory mode");
    const Bytes code = oras.Code();
    // what the application region must also hold: the code and its data as mapped (to 0x6AF000) and the 0xDF0000 normal heap
    const uint32_t others = 0x6AF000 - 0x100000 + 0xDF0000;
    if (linearHeap % 0x1000 || linearHeap < 0x2B48000 || linearHeap + others > total)
        throw FormatError("the linear heap must be a multiple of 0x1000, at least the game's 0x2B48000, and fit the mode with the code and the normal heap");

    std::vector<std::string> log;
    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path root = std::filesystem::path(outDir) / "load" / "mods" / id;
    std::filesystem::create_directories(root / "exefs");
    const Bytes exheader = ExHeaderWithSystemMode(oras.ExHeader(), mode);
    WriteFile((root / "exheader.bin").string(), exheader);
    log.push_back("exheader.bin: system mode " + std::to_string((int)mode) + " (" + std::to_string(total >> 20) + " MB of application memory)");
    const Bytes ips = CodePatchIps(code, {{0x106508, 0x02B48000, linearHeap, "the linear heap FUN_00106448 asks for"}});
    if (CodePatchApply(code, ips).size() != code.size()) throw FormatError("the code patch changed the code's size");
    WriteFile((root / "exefs" / "code.ips").string(), ips);
    char line[128];
    snprintf(line, sizeof line, "code.ips: linear heap 0x%X (%.1f MB, the game's 0x2B48000 = 43.3 MB)", linearHeap, linearHeap / 1048576.0);
    log.push_back(line);
    return log;
}

}
