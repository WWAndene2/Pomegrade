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
    uint32_t Vtable = 0, Slots = 0;       // X's vtable (the file offset of its slot 0) and its slot count
    uint32_t InsertSlot = 0, InsertValue = 0; // the slot Omega Ruby added and the .code function it holds
    uint32_t Constructor = 0, ObjectSize = 0; // the class's constructor (a file offset) and its object's size
    // an X import Omega Ruby has only changed (its arguments): the import resolved to Omega Ruby's function, but each of the
    // module's stubs of it (`ldr pc, [pc, #-4]` and the word the loader writes) made a branch to a thunk written after the
    // code: Prefix (setting what Omega Ruby's function takes more), then a jump to it
    struct Thunk { uint32_t XTarget, Target; std::vector<uint32_t> Prefix; std::string Why; };
    std::vector<Thunk> Thunks;
};

// a game's static module (static.crs) and DllField (the file, and where the linked program places it)
struct XyGameModules
{
    Bytes Static, Field;
    uint32_t FieldBase = 0;
};

// X's module ported: throws if a word or an import is not X's as described, or an import is not mapped. sound: X's sound
// id -> Omega Ruby's (XySounds::Translate)
Bytes XyPortModule(const Bytes& xModule, const XyGameModules& x, const XyGameModules& oras, const XyModulePort& port,
                   const std::function<uint32_t(uint32_t)>& sound);

// a ported gym: X's area kind (zone header word +0x26), the free Omega Ruby kind given it, its module
struct XyGym
{
    uint16_t XKind, OrasKind;
    const XyModulePort& Port;
};
const std::vector<XyGym>& XyGyms();

// Omega Ruby's area kind for an X/Y zone's (0xFFFF, none, when its module is not ported)
uint16_t XyGymAreaKind(uint16_t xKind);

// the DllField words loading the ported modules (DllField linked addresses, cave words expected zero): Field_LoadArea
// SpecificModule's switch (0x10273D30) sends the kinds Omega Ruby has no module for to its default case, whose first word
// (0x10274210, the kind still in r2; the switch's table is relocated by the loader, so not written) becomes a branch to a
// dispatcher sending each gym's kind to a routine in the cave doing what the game's cases do (read 20 October): load the
// module by name (the import
// stub 0x10243C48 of the module loader 0x36F284, which returns the module's handle, its mapped address: Omega Ruby's
// module list read on the running game, runs ro1-ro3), keep it at +0x1A4, allocate the object (0x102421F8, size and heap)
// and jump to its constructor (object, field), at the handle + the constructor's offset, as DllField has no import of it
std::vector<CodeWord> XyGymLoaderPatchAt(uint32_t cave);

// Grant's gym (X zone 160, Cyllage): an ambient sound whose volume follows the player's distance (DllUSGymRock, read 20
// October)
const XyModulePort& XyGymRock();

}

#endif // REMAKE_ORASXYGYMMODULES_H
