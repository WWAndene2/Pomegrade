// scratch: Azahar's LayeredFS (layered_fs.cpp) on Omega Ruby's real RomFS with the owner's load folder;
// the rebuilt RomFS is walked and a/0/1/4, a/0/3/9 read back through LayeredFS::ReadFile
#include "core/file_sys/layered_fs.h"
#include "N3dsRom.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
using namespace FileSys;

// the image's RomFS level 3, with a/0/3/9's bytes from the full copy (the sparse image lacks them)
class ImageReader : public RomFSReader {
public:
    struct Overlay { uint64_t at; std::vector<u8> data; };
    ImageReader(const char* image, uint64_t l3, uint64_t size, std::vector<Overlay> overlays)
        : f(image, std::ios::binary), l3(l3), size(size), overlays(std::move(overlays)) {}
    std::size_t GetSize() const override { return size; }
    std::size_t ReadFile(std::size_t offset, std::size_t length, u8* buffer) override {
        f.clear(); f.seekg(l3 + offset); f.read((char*)buffer, length);
        for (const Overlay& o : overlays) {
            const uint64_t a = std::max<uint64_t>(offset, o.at), b = std::min<uint64_t>(offset + length, o.at + o.data.size());
            if (a < b) std::memcpy(buffer + (a - offset), o.data.data() + (a - o.at), b - a);
        }
        return length;
    }
    bool AllowsCachedReads() const override { return false; }
    bool CacheReady(std::size_t, std::size_t) override { return false; }
private:
    std::ifstream f; uint64_t l3, size; std::vector<Overlay> overlays;
};

static std::vector<u8> Load(const char* p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
static u32 R32(const std::vector<u8>& b, size_t at) { u32 v; std::memcpy(&v, &b[at], 4); return v; }
static u64 R64(const std::vector<u8>& b, size_t at) { u64 v; std::memcpy(&v, &b[at], 8); return v; }
static std::string Name(const std::vector<u8>& b, size_t at, u32 len) { std::string s; for (u32 i = 0; i < len; i += 2) s += (char)b[at + i]; return s; }

int main(int argc, char** v) {
    // v: image l3 l3size mods_dir full_a039 want_a039 want_a014
    remake::N3dsRom game(v[1]);
    const uint64_t l3 = std::stoull(v[2]), l3size = std::stoull(v[3]);
    // v[5]: the game's real files the sparse image lacks, "path=file,path=file"
    std::vector<ImageReader::Overlay> overlays;
    std::string list = v[5];
    for (size_t at = 0; at < list.size();) {
        size_t end = list.find(',', at); if (end == std::string::npos) end = list.size();
        const std::string item = list.substr(at, end - at); const size_t eq = item.find('=');
        overlays.push_back({game.Files().at(item.substr(0, eq)).first - l3, Load(item.substr(eq + 1).c_str())});
        at = end + 1;
    }
    auto reader = std::make_shared<ImageReader>(v[1], l3, l3size, std::move(overlays));
    const std::string mods = v[4];
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
    int ok = 0;
    for (size_t o = 0; o < files.size();) {
        const u32 parent = R32(files, o), nameLen = R32(files, o + 0x1C);
        const std::string path = dirPath[parent] + Name(files, o + 0x20, nameLen);
        const u64 at = R64(files, o + 8), len = R64(files, o + 0x10);
        const char* want = nullptr;
        for (int a = 6; a < argc; a++) { const std::string arg = v[a]; const size_t eq = arg.find('='); if (arg.substr(0, eq) == path) want = v[a] + eq + 1; }
        if (want) {
            std::vector<u8> got(len); fs.ReadFile(dataAt + at, len, got.data());
            const bool same = got == Load(want);
            printf("%s read through LayeredFS: %llu bytes, identical to %s: %s\n", path.c_str(), (unsigned long long)len, want, same ? "yes" : "NO");
            ok += same;
        }
        o += 0x20 + ((nameLen + 3) & ~3u);
    }
    printf("%d of %d files checked identical\n", ok, argc - 6);
    return ok == argc - 6 ? 0 : 1;
}
