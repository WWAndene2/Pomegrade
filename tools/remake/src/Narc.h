#ifndef REMAKE_NARC_H
#define REMAKE_NARC_H

// NARC: the Nitro archive most DS games (Pokemon among them) pack their data
// in. A 16-byte header ("NARC"), then three sections: BTAF (member start/end
// offsets), BTNF (member names, usually none), GMIF (the members' data).

#include "Bytes.h"

#include <string>
#include <vector>

namespace remake
{

class Narc
{
public:
    explicit Narc(const Bytes& data);
    static bool Is(const Bytes& data);

    size_t Count() const { return Members.size(); }
    const Bytes& Member(size_t i) const { return Members.at(i); }
    // the member's name when the archive has a name table, else ""
    const std::string& Name(size_t i) const { return Names.at(i); }

private:
    std::vector<Bytes> Members;
    std::vector<std::string> Names;
};

}

#endif // REMAKE_NARC_H
