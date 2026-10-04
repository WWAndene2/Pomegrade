#ifndef REMAKE_NDSROM_H
#define REMAKE_NDSROM_H

// A DS cartridge image: its header and its file system (NitroFS: a file name
// table of directories and names, and a file allocation table of start/end
// offsets). Files the name table does not name (ARM9/ARM7 overlays) are listed
// as "overlay/<id>".

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

struct NdsFile
{
    uint16_t Id = 0;
    std::string Path;  // "fielddata/land_data/land_data.narc"
    uint32_t Start = 0, End = 0;
    uint32_t Size() const { return End - Start; }
};

class NdsRom
{
public:
    explicit NdsRom(Bytes image);

    const std::string& Title() const { return TitleText; }
    const std::string& GameCode() const { return Code; }  // "CPUE": Platinum (US)
    const std::vector<NdsFile>& Files() const { return FileList; }
    // nullptr when there is no such path
    const NdsFile* Find(const std::string& path) const;
    Bytes Read(const NdsFile& file) const;

private:
    Bytes Image;
    std::string TitleText, Code;
    std::vector<NdsFile> FileList;
    void ReadDirectory(uint32_t fnt, uint16_t dir, const std::string& prefix, std::vector<std::string>& names, int depth);
};

}

#endif // REMAKE_NDSROM_H
