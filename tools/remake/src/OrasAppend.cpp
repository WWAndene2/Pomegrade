#include "OrasAppend.h"
#include "BinLinker.h"
#include "CodePatch.h"
#include "Bps.h"
#include "Garc.h"
#include "NitroCompression.h"
#include "OrasZone.h"

#include <filesystem>

namespace remake
{

static Bytes Plain(const Bytes& data) { return IsLzCompressed(data) ? LzDecompress(data) : data; }
static void Put16(Bytes& b, size_t at, uint16_t v) { b.at(at) = (uint8_t)v; b.at(at + 1) = (uint8_t)(v >> 8); }

// the member appended as the source member is stored (its compressed bytes when the source's are), read back to check
static size_t Append(Garc& archive, const Bytes& stored)
{
    const size_t index = archive.Count();
    archive.Set(index, stored);
    if (Garc(archive.Write()).Sub(index) != stored) throw FormatError("the appended member does not read back identical");
    return index;
}

// a member rewritten from its plain bytes, compressed as it was
static void Rewrite(Garc& archive, size_t index, const Bytes& plain, bool compressed)
{
    archive.Set(index, compressed ? Lz11Compress(plain) : plain);
    if (Plain(Garc(archive.Write()).Sub(index)) != plain) throw FormatError("member " + std::to_string(index) + " does not read back identical");
}

std::vector<std::string> BuildAppendTest(N3dsRom& oras, AppendTest test, const std::string& outDir, size_t crowd)
{
    std::vector<std::string> log;
    if (test == AppendTest::Crowd)
    {
        const Bytes zones = oras.Read("a/0/1/3");
        const Garc gz(zones);
        Garc nz(zones);
        const Bytes plain = Plain(gz.Sub(6));
        OrasZone z = OrasZone::Read(plain);
        if (z.Characters.empty()) throw FormatError("zone 6 has no character to clone");
        const size_t own = z.Characters.size();
        // clones of the zone's characters, 8 a row from tile (92, 158), north-west of the Littleroot save's (100.5, 172.5)
        if (crowd > 255) throw FormatError("crowd: at most 255 characters (a count byte)");
        for (size_t i = own; i < crowd; i++)
        {
            ZoneCharacter c = z.Characters[i % own];
            c.Raw[20] = (uint16_t)(92 + (i - own) % 8 * 2);
            c.Raw[21] = (uint16_t)(158 + (i - own) / 8 * 2);
            z.Characters.push_back(c);
        }
        Rewrite(nz, 6, z.Write(plain), IsLzCompressed(gz.Sub(6)));
        log.push_back("zone 6: " + std::to_string(own) + " characters -> " + std::to_string(z.Characters.size()));
        char id[17];
        snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
        const std::filesystem::path path = std::filesystem::path(outDir) / "load" / "mods" / id / "romfs_ext" / "a/0/1/3.bps";
        const Bytes data = nz.Write(), bps = BpsCreate(zones, data);
        if (BpsApply(zones, bps) != data) throw FormatError("a/0/1/3: the patch does not rebuild the file");
        std::filesystem::create_directories(path.parent_path());
        WriteFile(path.string(), bps);
        return log;
    }
    const Bytes pieces = oras.Read("a/0/3/9"), matrices = oras.Read("a/0/4/0"), zones = oras.Read("a/0/1/3");
    const Garc gp(pieces), gm(matrices), gz(zones);
    Garc np(pieces), nm(matrices), nz(zones);
    const size_t piece = Append(np, gp.Sub(6)), matrix = Append(nm, gm.Sub(1));
    // the added zone names itself (header word 13, the zone's own number: section 3)
    Bytes zone = Plain(gz.Sub(6));
    BinLinker zc = BinLinker::Read(zone, "ZO");
    const size_t zoneIndex = gz.Count();
    Put16(zc.Files.at(0), 13 * 2, (uint16_t)zoneIndex);
    zone = zc.Write();
    if (OrasZone::Read(zone).Number() != (int)zoneIndex) throw FormatError("the added zone does not read its own number");
    Append(nz, IsLzCompressed(gz.Sub(6)) ? Lz11Compress(zone) : zone);
    log.push_back("appended: piece " + std::to_string(piece) + " (Littleroot's), matrix " + std::to_string(matrix) + " (matrix 1), zone " +
                  std::to_string(zoneIndex) + " (zone 6, own number " + std::to_string(zoneIndex) + ")");

    const bool matrixCompressed = IsLzCompressed(gm.Sub(1));
    if (test == AppendTest::Piece || test == AppendTest::Zone || test == AppendTest::ZoneRaised)
    {
        Bytes m = Plain(gm.Sub(1));
        BinLinker mc = BinLinker::Read(m, "MM");
        Bytes& f = mc.Files.at(0);
        const size_t w = U16(f, 4), h = U16(f, 6);
        if (test == AppendTest::Piece)
        {
            const size_t at = 8 + 2 * (4 * w + 2); // cell (2, 4)
            if (U16(f, at) != 6) throw FormatError("matrix 1's cell (2, 4) is not piece 6");
            Put16(f, at, (uint16_t)piece);
            log.push_back("matrix 1: cell (2, 4) now piece " + std::to_string(piece));
        }
        else
        {
            size_t moved = 0;
            const size_t zoneAt = 8 + 2 * w * h;
            for (size_t k = 0; k < 16 * w * h; k++)
                if (U16(f, zoneAt + 2 * k) == 6) { Put16(f, zoneAt + 2 * k, (uint16_t)zoneIndex); moved++; }
            if (!moved) throw FormatError("matrix 1's zone grid has no block of zone 6");
            log.push_back("matrix 1: " + std::to_string(moved) + " blocks of zone 6 now zone " + std::to_string(zoneIndex));
        }
        Rewrite(nm, 1, mc.Write(), matrixCompressed);
    }
    if (test == AppendTest::Matrix)
    {
        Bytes z = Plain(gz.Sub(6));
        BinLinker c = BinLinker::Read(z, "ZO");
        Put16(c.Files.at(0), 2 * 2, (uint16_t)matrix);
        z = c.Write();
        if (OrasZone::Read(z).Matrix() != (int)matrix) throw FormatError("zone 6 does not read its new matrix");
        Rewrite(nz, 6, z, IsLzCompressed(gz.Sub(6)));
        log.push_back("zone 6: matrix " + std::to_string(matrix));
    }

    Bytes codePatch;
    if (test == AppendTest::ZoneRaised)
    {
        // the zone header table (member 536): one 56-byte row per zone number, read whole into a buffer of exactly its size
        // (FUN_00112b50); rows 536 and 537 stand for the two table members and are never a zone, row 538 is the new zone's
        const size_t rows = zoneIndex + 1, rowBytes = 56;
        Bytes table = Plain(gz.Sub(536));
        if (table.size() != 536 * rowBytes) throw FormatError("member 536 is not the 536-row zone header table");
        const Bytes header = BinLinker::Read(zone, "ZO").Files.at(0);
        if (header.size() != rowBytes) throw FormatError("the zone's header is not 56 bytes");
        const Bytes row6(table.begin() + 6 * rowBytes, table.begin() + 7 * rowBytes);
        while (table.size() < zoneIndex * rowBytes) table.insert(table.end(), row6.begin(), row6.end());
        table.insert(table.end(), header.begin(), header.end());
        Rewrite(nz, 536, table, IsLzCompressed(gz.Sub(536)));
        // the encounter container (member 537): one file per zone number, zone 6's for the added ones
        BinLinker encounters = BinLinker::Read(Plain(gz.Sub(537)), "EN");
        if (encounters.Files.size() != 536) throw FormatError("member 537 is not the 536-file encounter container");
        while (encounters.Files.size() < rows) encounters.Files.push_back(encounters.Files.at(6));
        Rewrite(nz, 537, encounters.Write(), IsLzCompressed(gz.Sub(537)));
        log.push_back("zone tables: member 536 " + std::to_string(rows) + " rows (" + std::to_string(table.size()) + " bytes), member 537 " +
                      std::to_string(encounters.Files.size()) + " files");
        // the code: the header loaders' bound (cmp rN, #0x218 -> #0x400: zones below 1024; an ARM immediate, 0x400 = 1 ror 22)
        // and the table's exact size (the word FUN_00112b50 allocates and checks)
        codePatch = CodePatchIps(oras.Code(), {
            {0x3D9774, 0xE3550F86, 0xE3550B01, "FUN_003d9740's zone bound"},
            {0x3D99F8, 0xE3560F86, 0xE3560B01, "FUN_003d99b8's zone bound"},
            {0x112C0C, 0x00007540, (uint32_t)table.size(), "the zone header table's size"},
        });
        log.push_back("code: zone bound 1024, header table " + std::to_string(table.size()) + " bytes (3 words, exefs/code.ips)");
    }

    char id[17];
    snprintf(id, sizeof id, "%016llX", (unsigned long long)oras.ProgramId());
    const std::filesystem::path root = std::filesystem::path(outDir) / "load" / "mods" / id / "romfs_ext";
    for (const auto& [path, archive, original] : {std::tuple<const char*, Garc*, const Bytes*>{"a/0/3/9", &np, &pieces}, {"a/0/4/0", &nm, &matrices}, {"a/0/1/3", &nz, &zones}})
    {
        const Bytes data = archive->Write();
        const Bytes bps = BpsCreate(*original, data);
        if (BpsApply(*original, bps) != data) throw FormatError(std::string(path) + ": the patch does not rebuild the file");
        std::filesystem::create_directories((root / path).parent_path());
        WriteFile((root / (std::string(path) + ".bps")).string(), bps);
        log.push_back(std::string(path) + ": " + std::to_string(Garc(data).Count()) + " members, patch " + std::to_string(bps.size()) + " bytes, checked");
    }
    if (!codePatch.empty())
    {
        const std::filesystem::path ips = std::filesystem::path(outDir) / "load" / "mods" / id / "exefs" / "code.ips";
        std::filesystem::create_directories(ips.parent_path());
        WriteFile(ips.string(), codePatch);
    }
    return log;
}

}
