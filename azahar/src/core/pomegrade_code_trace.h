// Copyright Pomegrade
// Licensed under GPLv2 or any later version

#pragma once

// Pomegrade code trace: a debugging record of what the running game's code does, for finding
// where a game (a mod) freezes. Turned on in Pomegrade's settings (3DS > Record a code trace);
// while a game runs it keeps, in memory, the last events of each kind:
//   - kernel calls (SVC): thread, address, call;
//   - system service requests: service, request, its first words (a file read's offset and size);
//   - files and archives opened;
//   - code modules (CRO) loaded and where;
//   - several times a second, every thread of the game: state, PC, LR, SP and the code
//     addresses on its stack (the call chain, read without frame pointers).
// Written to <folder>/<program id>_<date>/ when the game stops, and every 30 seconds while it
// runs (a frozen game that is killed keeps its last record). Off: one test of a flag per event.

#include <string>
#include "common/common_types.h"

namespace Core {
class System;
}

namespace Pomegrade::CodeTrace {

/// Turns the trace on for the next game (folder: where the record is written), or off.
void Configure(bool enabled, const std::string& folder);

bool Enabled();

/// The game starts: the record starts empty.
void Start(Core::System& system, u64 program_id);

/// The game stops: the record is written.
void Stop();

void KernelCall(Core::System& system, u32 id, const char* name);
void ServiceRequest(Core::System& system, const std::string& service, const char* request,
                    const u32* command, std::size_t words);
void FileOpened(Core::System& system, const char* what, const std::string& path);
void ModuleLoaded(Core::System& system, const std::string& name, u32 address, u32 end);

/// Called from the emulation loop: takes the threads' snapshot when it is due, and writes the
/// record every 30 seconds.
void Tick(Core::System& system);

} // namespace Pomegrade::CodeTrace
