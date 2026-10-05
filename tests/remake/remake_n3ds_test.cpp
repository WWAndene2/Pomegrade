// The remake tooling's 3DS side: GARC written back, ORAS's containers
// (BinLinker) with their alignment, LZ11 compression, and a decrypted 3DS
// image's RomFS, all built here to the layouts checked on Omega Ruby (see
// Garc.h, BinLinker.h, N3dsRom.h). On the real game the same code rewrites
// a/0/3/9 and a/0/4/0, their 857 GR and 431 MM containers, byte-identical.
#include "BinLinker.h"
#include "Garc.h"
#include "N3dsRom.h"
#include "NitroCompression.h"
#include "synthetic_files.h"

#include <cstdio>
#include <string>

using namespace remake;
using namespace synthetic;

static bool ok = true;
static void check(bool cond, const std::string& what)
{
    printf("%s: %s\n", what.c_str(), cond ? "yes" : "NO");
    if (!cond) ok = false;
}

// a RomFS name: UTF-16LE, padded to 4 bytes
static void PushName(Bytes& b, const std::string& s)
{
    for (char c : s) Push16(b, (uint16_t)c);
    while (b.size() % 4) b.push_back(0);
}

// a decrypted 3DS image (NCSD, one NCCH) whose RomFS holds "top.bin" and "a/0/x.bin"
static Bytes Image(bool encrypted)
{
    const Bytes top = {1, 2, 3}, x = {'G', 'A', 'R', 'C', 5};
    // level 3: header, directory table, file table, data
    Bytes dirs, files, data;
    auto dir = [&](uint32_t parent, uint32_t sibling, uint32_t child, uint32_t file, const std::string& name) {
        const uint32_t at = (uint32_t)dirs.size();
        Push32(dirs, parent); Push32(dirs, sibling); Push32(dirs, child); Push32(dirs, file); Push32(dirs, 0xFFFFFFFF);
        Push32(dirs, (uint32_t)name.size() * 2); PushName(dirs, name);
        return at;
    };
    auto file = [&](uint32_t parent, const std::string& name, const Bytes& content) {
        const uint32_t at = (uint32_t)files.size();
        Push32(files, parent); Push32(files, 0xFFFFFFFF);
        Push32(files, (uint32_t)data.size()); Push32(files, 0); Push32(files, (uint32_t)content.size()); Push32(files, 0);
        Push32(files, 0xFFFFFFFF); Push32(files, (uint32_t)name.size() * 2); PushName(files, name);
        data.insert(data.end(), content.begin(), content.end());
        while (data.size() % 16) data.push_back(0);
        return at;
    };
    const uint32_t topFile = file(0, "top.bin", top);
    const uint32_t root = dir(0, 0xFFFFFFFF, 0, topFile, "");    // its child is patched below
    const uint32_t a = dir(root, 0xFFFFFFFF, 0, 0xFFFFFFFF, "a");
    const uint32_t a0 = dir(a, 0xFFFFFFFF, 0xFFFFFFFF, 0, "0");
    const uint32_t xFile = file(a0, "x.bin", x);
    Put32(dirs, root + 8, a); Put32(dirs, a + 8, a0); Put32(dirs, a0 + 12, xFile);
    Bytes level3;
    const uint32_t dirAt = 0x28, fileAt = dirAt + (uint32_t)dirs.size(), dataAt = (fileAt + (uint32_t)files.size() + 15) / 16 * 16;
    for (uint32_t v : {0x28u, 0u, 0u, dirAt, (uint32_t)dirs.size(), 0u, 0u, fileAt, (uint32_t)files.size(), dataAt}) Push32(level3, v);
    level3.insert(level3.end(), dirs.begin(), dirs.end());
    level3.insert(level3.end(), files.begin(), files.end());
    level3.resize(dataAt, 0);
    level3.insert(level3.end(), data.begin(), data.end());

    // IVFC: master hash 0x20 bytes, level 3 blocks of 0x1000: level 3 at 0x1000
    Bytes romfs(0x1000, 0);
    memcpy(romfs.data(), "IVFC", 4); Put32(romfs, 8, 0x20); Put32(romfs, 0x4C, 12);
    romfs.insert(romfs.end(), level3.begin(), level3.end());
    while (romfs.size() % 0x200) romfs.push_back(0);

    Bytes ncch(0x200, 0);
    memcpy(ncch.data() + 0x100, "NCCH", 4);
    Put32(ncch, 0x118, 0x0011C400); Put32(ncch, 0x11C, 0x00040000); // program id 000400000011C400
    memcpy(ncch.data() + 0x150, "CTR-P-TEST", 10);
    ncch[0x18F] = encrypted ? 0 : 0x04;
    Put32(ncch, 0x1B0, 1); Put32(ncch, 0x1B4, (uint32_t)(romfs.size() / 0x200));
    ncch.insert(ncch.end(), romfs.begin(), romfs.end());

    Bytes img(0x200, 0);
    memcpy(img.data() + 0x100, "NCSD", 4);
    Put32(img, 0x120, 1); Put32(img, 0x124, (uint32_t)(ncch.size() / 0x200));
    img.insert(img.end(), ncch.begin(), ncch.end());
    return img;
}

int main()
{
    // GARC: built, written, read back; a sub-file of 5 bytes padded with 0xFF, its length kept
    Garc g(Bytes{'C', 'R', 'A', 'G', 0x1C, 0, 0, 0, 0xFF, 0xFE, 0, 4, 4, 0, 0, 0, 0x28, 0, 0, 0, 0x28, 0, 0, 0, 0, 0, 0, 0,
                 'O', 'T', 'A', 'F', 12, 0, 0, 0, 0, 0, 0xFF, 0xFF, 'B', 'T', 'A', 'F', 12, 0, 0, 0, 0, 0, 0, 0,
                 'B', 'M', 'I', 'F', 12, 0, 0, 0, 0, 0, 0, 0});
    check(g.Count() == 0 && g.Version() == 0x0400, "GARC: an empty archive read");
    g.Set(0, Bytes{1, 2, 3, 4, 5});
    g.Set(1, Bytes{6});
    g.Set(1, Bytes{7, 8}, 2); // a second sub-file (bit 2)
    const Bytes w = g.Write();
    const Garc r(w);
    check(r.Count() == 2 && r.Sub(0) == Bytes({1, 2, 3, 4, 5}) && r.Sub(1) == Bytes({6}) && r.Has(1, 2) && !r.Has(1, 1) && r.Sub(1, 2) == Bytes({7, 8}),
          "GARC: written and read back (sub-files, masks)");
    check(U32(w, 0x18) == 5 && w[U32(w, 0x10) + 5] == 0xFF && w[U32(w, 0x10) + 7] == 0xFF && w.size() == U32(w, 0x14),
          "GARC: largest sub-file, 0xFF padding, file size in the header");
    check(r.Write() == w, "GARC: rewritten byte-identical");
    bool refused = false;
    try { Bytes bad = w; Put32(bad, 0x1C + 12 + 8 + 12 + 4 + 8, 100); Garc x(bad); } catch (const FormatError&) { refused = true; }
    check(refused, "GARC: a length past its range refused");

    // BinLinker: alignment measured and kept
    BinLinker mm{"MM", {Bytes{1, 2, 3, 4, 5, 6}, Bytes{7, 8, 9, 10}}, 4};
    const Bytes mmData = mm.Write();
    check(U32(mmData, 4) == 16 && U32(mmData, 8) == 24 && U32(mmData, 12) == 28 && mmData.size() == 28, "container: 4-byte alignment");
    BinLinker gr{"GR", {Bytes(6, 1), Bytes(130, 2)}, 0x80};
    const Bytes grData = gr.Write();
    check(U32(grData, 4) == 0x80 && U32(grData, 8) == 0x100 && U32(grData, 12) == 0x200 && grData.size() == 0x200, "container: 0x80 alignment (as ORAS's GR)");
    const BinLinker grBack = BinLinker::Read(grData, "GR");
    check(grBack.Align == 0x80 && grBack.Write() == grData && grBack.Files[0].size() == 0x80 && grBack.Files[0][5] == 1 && grBack.Files[0][6] == 0,
          "container: read back, alignment measured, rewritten identical");
    refused = false;
    try { BinLinker::Read(grData, "MM"); } catch (const FormatError&) { refused = true; }
    check(refused, "container: the wrong tag refused");

    // LZ11: literals, short, medium and long references (past 0x110 and 0x10110), large sizes
    Bytes text;
    for (int i = 0; i < 5000; i++) text.push_back((uint8_t)("pomegranate"[i % 11] + (i / 997)));
    Bytes runs(200000, 7);
    for (size_t i = 0; i < runs.size(); i += 70001) runs[i] = 9;
    Bytes noise;
    uint32_t s = 1;
    for (int i = 0; i < 4096; i++) { s = s * 1103515245 + 12345; noise.push_back((uint8_t)(s >> 16)); }
    bool lz = true;
    for (const Bytes* b : {&text, &runs, &noise})
    {
        const Bytes c = Lz11Compress(*b);
        lz = lz && c[0] == 0x11 && LzDecompress(c) == *b;
    }
    check(lz, "LZ11: text, long runs, noise decompress back identical");
    check(Lz11Compress(runs).size() < 200, "LZ11: long runs use the long references (200,000 bytes in under 200)");
    Bytes big(0x1000001, 3);
    const Bytes bigC = Lz11Compress(big);
    check(bigC[1] == 0 && bigC[2] == 0 && bigC[3] == 0 && U32(bigC, 4) == big.size() && LzDecompress(bigC) == big, "LZ11: a size past 24 bits in the 32-bit field");

    // a decrypted 3DS image
    WriteFile("n3ds_test.3ds", Image(false));
    N3dsRom game("n3ds_test.3ds");
    check(game.ProgramId() == 0x000400000011C400ull && game.ProductCode() == "CTR-P-TEST", "3DS image: program id, product code");
    check(game.Files().size() == 2 && game.Has("top.bin") && game.Has("a/0/x.bin"), "3DS image: the RomFS files, with their directories");
    check(game.Read("top.bin") == Bytes({1, 2, 3}) && game.Read("a/0/x.bin") == Bytes({'G', 'A', 'R', 'C', 5}), "3DS image: files read");
    refused = false;
    try { game.Read("a/0/y.bin"); } catch (const FormatError&) { refused = true; }
    check(refused, "3DS image: a missing path refused");
    WriteFile("n3ds_test.3ds", Image(true));
    refused = false;
    try { N3dsRom enc("n3ds_test.3ds"); } catch (const FormatError& e) { refused = std::string(e.what()).find("encrypted") != std::string::npos; }
    check(refused, "3DS image: an encrypted image refused, saying so");
    std::remove("n3ds_test.3ds");

    printf(ok ? "ALL OK\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
