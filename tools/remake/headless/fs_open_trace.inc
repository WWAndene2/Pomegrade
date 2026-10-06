    // Pomegrade headless trace (put in place of OpenFile's LOG_DEBUG in the runner's copy of fs_user.cpp by the Remake
    // workflow only): every file the game opens, with the code that asked for it. The return address and the code
    // addresses on the client thread's stack name the game's function that picks a file (an archive by its number),
    // where a Sinnoh story would have to pick its own files.
    {
        const auto thread = ctx.ClientThread();
        const auto process = thread->owner_process.lock();
        auto& memory = Core::System::GetInstance().Memory();
        const u32 sp = thread->context.GetStackPointer();
        std::string chain;
        int found = 0;
        for (u32 at = sp; process && at < sp + 0x400 && found < 12; at += 4) {
            if (!memory.IsValidVirtualAddress(*process, at)) break;
            const u32 word = memory.Read32(*process, at);
            if ((word >= 0x100000 && word < 0x630000) || (word >= 0x6F0000 && word < 0x900000)) {
                chain += fmt::format(" {:08X}", word);
                found++;
            }
        }
        LOG_WARNING(Service_FS, "pomegrade open {} lr {:08X} stack{}", file_path.DebugStr(),
                    thread->context.GetLinkRegister(), chain);
    }
