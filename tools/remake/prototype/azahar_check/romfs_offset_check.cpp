// Which file of a mod's RomFS, as Azahar's LayeredFS rebuilds it (layered_fs.cpp, the real code), holds a
// RomFS offset, and for a GARC archive which member: the offsets a code trace's file reads give (the game
// opens its whole RomFS as one file, SelfNCCH, and reads it at RomFS level 3 offsets).
//   romfs_offset_check <oras.3ds> <mod folder: load/mods/<program id>/> <offset> [<offset>...]
#include "core/file_sys/layered_fs.h"
#include "N3dsRom.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
using namespace FileSys;

// the image's RomFS level 3, as Azahar's RomFS reader serves it
class ImageReader : public RomFSReader {
public:
    ImageReader(const char* image, uint64_t l3, uint64_t size) : f(image, std::ios::binary), l3(l3), size(size) {}
    std::size_t GetSize() const override { return size; }
    std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer) override {
        f.clear(); f.seekg(l3 + offset); f.read((char*)buffer, length);
        return length;
    }
    bool AllowsCachedReads() const override { return false; }
    bool CacheReady(std::size_t, std::size_t) override { return false; }
private:
    std::ifstream f; uint64_t l3, size;
};

static u32 R32(const std::vector<u8>& b, size_t at) { u32 v; std::memcpy(&v, &b[at], 4); return v; }
static u64 R64(const std::vector<u8>& b, size_t at) { u64 v; std::memcpy(&v, &b[at], 8); return v; }
static std::string Name(const std::vector<u8>& b, size_t at, u32 len) { std::string s; for (u32 i = 0; i < len; i += 2) s += (char)b[at + i]; return s; }

// a GARC's member holding byte `at` of the archive (its FATB entries, offsets from the data start)
static void GarcMember(LayeredFS& fs, u64 fileAt, u64 fileLen, u64 at) {
    std::vector<u8> head(0x1C);
    fs.ReadFile(fileAt, head.size(), head.data());
    if (std::memcmp(head.data(), "CRAG", 4) != 0) { printf("  not a GARC\n"); return; }
    const u32 headerSize = R32(head, 4), dataStart = R32(head, 0x10);
    std::vector<u8> fato(12);
    fs.ReadFile(fileAt + headerSize, 12, fato.data());
    const u32 fatoSize = R32(fato, 4);
    std::vector<u8> fatb(dataStart - headerSize - fatoSize);
    fs.ReadFile(fileAt + headerSize + fatoSize, fatb.size(), fatb.data());
    const u32 count = R32(fatb, 8);
    size_t o = 12;
    for (u32 i = 0; i < count; i++) {
        const u32 mask = R32(fatb, o); o += 4;
        for (int sub = 0; sub < 32; sub++) {
            if (!(mask >> sub & 1)) continue;
            const u32 start = R32(fatb, o), end = R32(fatb, o + 4), len = R32(fatb, o + 8); o += 12;
            const u64 from = dataStart + start, to = dataStart + end;
            if (at >= from && at < to)
                printf("  GARC member %u%s: archive bytes 0x%llX-0x%llX (%u bytes), offset 0x%llX into it\n", i,
                       sub ? (" file " + std::to_string(sub)).c_str() : "", (unsigned long long)from, (unsigned long long)to, len,
                       (unsigned long long)(at - from));
        }
    }
    (void)fileLen;
}

int main(int argc, char** v) {
    if (argc < 4) { fprintf(stderr, "usage: romfs_offset_check <oras.3ds> <mod folder> <offset>...\n"); return 2; }
    remake::N3dsRom game(v[1]);
    auto reader = std::make_shared<ImageReader>(v[1], game.Level3(), game.Size() - game.Level3());
    const std::string mods = v[2];
    LayeredFS fs(reader, mods + "romfs/", mods + "romfs_ext/");

    std::vector<u8> head(0x28); fs.ReadFile(0, 0x28, head.data());
    const u32 dirAt = R32(head, 0x0C), dirLen = R32(head, 0x10), fileAt = R32(head, 0x1C), fileLen = R32(head, 0x20), dataAt = R32(head, 0x24);
    std::vector<u8> dirs(dirLen), files(fileLen);
    fs.ReadFile(dirAt, dirLen, dirs.data()); fs.ReadFile(fileAt, fileLen, files.data());
    std::map<u32, std::string> dirPath;
    for (size_t o = 0; o < dirs.size();) {
        const u32 parent = R32(dirs, o), nameLen = R32(dirs, o + 0x14);
        dirPath[o] = o == 0 ? "" : dirPath[parent] + Name(dirs, o + 0x18, nameLen) + "/";
        o += 0x18 + ((nameLen + 3) & ~3u);
    }
    printf("LayeredFS RomFS: %zu bytes, file data from 0x%X\n", fs.GetSize(), dataAt);
    for (int a = 3; a < argc; a++) {
        const u64 want = std::stoull(v[a], nullptr, 0);
        bool found = false;
        for (size_t o = 0; o < files.size();) {
            const u32 parent = R32(files, o), nameLen = R32(files, o + 0x1C);
            const u64 at = dataAt + R64(files, o + 8), len = R64(files, o + 0x10);
            if (want >= at && want < at + len) {
                printf("0x%llX: %s%s, offset 0x%llX of its %llu bytes\n", (unsigned long long)want, dirPath[parent].c_str(),
                       Name(files, o + 0x20, nameLen).c_str(), (unsigned long long)(want - at), (unsigned long long)len);
                GarcMember(fs, at, len, want - at);
                found = true;
            }
            o += 0x20 + ((nameLen + 3) & ~3u);
        }
        if (!found) printf("0x%llX: in no file (metadata or padding)\n", (unsigned long long)want);
    }
    return 0;
}
