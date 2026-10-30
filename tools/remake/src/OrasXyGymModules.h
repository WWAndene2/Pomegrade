#ifndef REMAKE_ORASXYGYMMODULES_H
#define REMAKE_ORASXYGYMMODULES_H

// X/Y's gym mechanics given to Omega Ruby (DEVELOPMENT_NOTE.md 3e, lane B step 1): X/Y keeps each gym's mechanics in a code
// module of its own, DllUSGym<Type>.cro, which X's DllField loads for the zones whose header word +0x26 (the area kind)
// names it, as Omega Ruby's Field_LoadAreaSpecificModule (0x10273D30) loads its DllUS modules (read 20 October: X's switch
// 0x1023B52C, kinds 0 Fighting, 1 Fairy, 2 Insect, 3 Rock, 4 Grass, 0xD Esper, 0xE Ice, 0x19-0x1B Elec; Omega Ruby's kinds
// 1, 2, 9, 12, 17-20 and 22 load nothing). Each is ported rather than rewritten: X's module, with
//  - its imports pointed at Omega Ruby's same functions: a CRO names an import by a segment tag of the module exporting it
//    (|static|, the .code, or DllField), so each tag is rewritten to Omega Ruby's address, found by
//    prototype/xy_import_match.py and read where it found only a changed copy;
//  - its own words that hold X's layouts patched: the player object's position (+0xC64 in X, +0x6F8 in Omega Ruby: X's
//    0x1029B53C and Omega Ruby's 0x102DC288 read side by side), X's sound ids (OrasXySound.h);
//  - its class's vtable given the slot Omega Ruby added: the area module class has 13 virtual methods in X and 14 in Omega
//    Ruby, a "return 0" at slot 12 (both games' DllUSPokecen compared). The new vtable is written in the free bytes after
//    the code segment, which grows to hold it, and the module's own relocations of the old one are pointed at it, so no
//    table of the module grows;
//  - renamed (DllXy..., the same length), as Omega Ruby has modules of X's names.
// Azahar only: a console checks a module's hash against the program's list (Azahar does not, CROHelper::VerifyHash).

#include "Bytes.h"
#include "CodePatch.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace remake
{

// what XyPortModule changes in an X module
struct XyModulePort
{
    std::string XName, Name;              // X's module name and the ported one's (the same length)
    std::map<uint32_t, uint32_t> Static;  // an X .code address imported -> Omega Ruby's
    std::map<uint32_t, uint32_t> Field;   // an X DllField address imported (linked, prototype/cro_link.py) -> Omega Ruby's
    struct Word { uint32_t Offset, Expected, Value; std::string Why; };
    std::vector<Word> Words;              // the module's own words (file offsets) changed
    std::vector<std::pair<uint32_t, uint32_t>> Sounds; // a word holding an X sound id (file offset, the id): translated
    std::vector<uint32_t> Vtables;        // X's vtables of the area module class (the file offset of each one's slot 0)
    uint32_t Slots = 0;                   // their slot count in X
    uint32_t InsertSlot = 0, InsertValue = 0; // the slot Omega Ruby added and the .code function it holds
    struct Class { uint32_t Constructor, ObjectSize; }; // a constructor (a file offset) and its object's size
    std::vector<Class> Classes;           // one per area kind loading the module (Clemont's has three)
    // an X import Omega Ruby has only changed (its arguments): the import resolved to Omega Ruby's function, but each of the
    // module's stubs of it (`ldr pc, [pc, #-4]` and the word the loader writes) made a branch to a thunk written after the
    // code: Prefix (setting what Omega Ruby's function takes more), then a jump to it
    struct Thunk { uint32_t XTarget, Target; std::vector<uint32_t> Prefix; std::string Why; };
    std::vector<Thunk> Thunks;
    // X's .code functions Omega Ruby has no copy of (xy_only_code.py), copied after the code (30 October, Viola's): each
    // its X address and bytes (its literal pool and any data it addresses from the pc included). In the copies, a branch
    // to another copy stays relative; one to X's .code goes to a veneer (`ldr pc, [pc, #-4]` and Omega Ruby's address
    // from Static: the module runs where the heap has room, the .code where it is linked, so never a direct branch); a
    // literal naming an X address must be in Literals (X's -> Omega Ruby's), else the port throws; an import of a copied
    // function has its stubs made branches to the copy
    struct Copy { uint32_t XAddress, Size; std::string Why; };
    std::vector<Copy> Copies;
    std::map<uint32_t, uint32_t> Literals; // a literal of the copies: an X .code or data address -> Omega Ruby's
    // the archive the module loads (X's number, a/X/Y/Z by its digits; 0 none) and its words requesting it (`mov rd, #n`,
    // file offsets): given Omega Ruby's number for it (XyGymArchiveNumber)
    uint16_t Archive = 0;
    std::vector<uint32_t> ArchiveWords;
};

// a game's static module (static.crs) and DllField (the file, and where the linked program places it)
struct XyGameModules
{
    Bytes Static, Field;
    uint32_t FieldBase = 0;
    Bytes Code; // its .code (from 0x100000): what a port's Copies copy (X's); may be empty when nothing is copied
};

// X's module ported: throws if a word or an import is not X's as described, or an import is not mapped. sound: X's sound
// id -> Omega Ruby's (XySounds::Translate)
Bytes XyPortModule(const Bytes& xModule, const XyGameModules& x, const XyGameModules& oras, const XyModulePort& port,
                   const std::function<uint32_t(uint32_t)>& sound, uint32_t archive = 0);

// the Omega Ruby number given the k-th ported archive (k from 0): past the game's 299 (oras-engine's longer path table,
// the room), an ARM immediate for the modules' `mov r1, #n` (0x130, 0x140...); its path rom:/a/X/Y/Z by its digits
uint32_t XyGymArchiveNumber(size_t k);
// the number the module's archive gets (its place among the ported modules with one)
uint32_t XyGymArchiveNumberOf(const XyModulePort& port);
std::string XyArchivePath(uint32_t number); // "a/X/Y/Z"

// a gym's archive (a GARC: X's Ice one is one LZ11-compressed "TM" container of the 48 BCH its loader unpacks) made
// Omega Ruby's: each BCH, a member or in a member's 2-letter container, converted to Omega Ruby's version (BchUpgrade.h;
// Omega Ruby's model code read X's BCH 0x07 as nothing: unmapped reads in Gfx_ModelBinding_BindResources 0x374A94, run
// gi1; none once converted, run gi2); a compressed member decompressed and compressed again; a file the conversion
// cannot carry kept, with a report line
Bytes XyGymArchiveToOras(const Bytes& garc, std::vector<std::string>& report);

// a ported gym: X's area kind (zone header word +0x26), the free Omega Ruby kind given it, its module and the class it
// builds (Port.Classes)
struct XyGym
{
    uint16_t XKind, OrasKind;
    const XyModulePort& Port;
    size_t Class = 0;
};
const std::vector<XyGym>& XyGyms();

// Omega Ruby's area kind for an X/Y zone's (0xFFFF, none, when its module is not ported)
uint16_t XyGymAreaKind(uint16_t xKind);

// the DllField words loading the ported modules (DllField linked addresses, cave words expected zero): Field_LoadArea
// SpecificModule's switch (0x10273D30) sends the kinds Omega Ruby has no module for to its default case, whose first word
// (0x10274210, the kind still in r2; the switch's table is relocated by the loader, so not written) becomes a branch to a
// dispatcher sending each gym's kind, with its record (constructor, object size, path), to a routine in the cave doing
// what the game's cases do (read 20 October): load the module by name (the import
// stub 0x10243C48 of the module loader 0x36F284, which returns the module's handle, its mapped address: Omega Ruby's
// module list read on the running game, runs ro1-ro3), keep it at +0x1A4, allocate the object (0x102421F8, size and heap)
// and jump to its constructor (object, field), at the handle + the constructor's offset, as DllField has no import of it
std::vector<CodeWord> XyGymLoaderPatchAt(uint32_t cave);

// Grant's gym (X zone 160, Cyllage): an ambient sound whose volume follows the player's distance (DllUSGymRock, read 20
// October)
const XyModulePort& XyGymRock();

// Wulfric's gym (X zone 226, Snowbelle; DllUSGymIce, read 20 October): checked headless in Vaniville's bedroom (given its
// kind for the check only, run gi2): the module and its archive load, its update runs each frame, no fault
const XyModulePort& XyGymIce();

// Clemont's gym (X zones 77-82, Lumiose; DllUSGymElec, read 20 October): its lifts and doors, three classes (X's area kinds
// 0x19-0x1B); mapped and ported, not run in the game
const XyModulePort& XyGymElec();

// Viola's gym (X zone 56, Santalune; DllUSGymInsect, mapped 30 October): its imports mapped (xy_import_match.py,
// xy_callee_map.py), its two X-own helpers and their callees copied (Copies); `xy-gym-module ... insect` builds it.
// In XyGyms (X's kind 2 -> 19) since 31 October: its dew drops' archive is X's a/1/1/0 (one "TM" container of 17 CGFX,
// ids 0x35-0x45), given to it as the other gyms' archives; without it Omega Ruby's a/1/1/0 was read, the 17 resource
// handles stayed unresolved and the dew drops' build read null (runs vI1, vD1, 30 October). Run vA2 (31 October): the
// gym shows, web, dew drops, people and the player, the game going on. Left: an unmapped read at 0x95 every frame
// (Omega Ruby's 0x4CCB9C reads [model + 0x28] + 0x94, the model's + 0x28 still the flag 1 its create method writes,
// the same in both games: the model the helper at + 0x43C draws (the object's + 0x2C) is not bound yet, slots 0xC /
// 0x14 write its + 0x28); not checked: why, and whether X reaches that read the same way
const XyModulePort& XyGymInsect();

}

#endif // REMAKE_ORASXYGYMMODULES_H
