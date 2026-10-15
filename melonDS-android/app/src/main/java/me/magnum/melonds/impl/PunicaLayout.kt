package me.magnum.melonds.impl

import me.magnum.melonds.domain.model.rom.RomPlatform
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * The Punica folder's layout (owner's decision, 15 October): one folder per console, the same six folders in each.
 *
 * ```
 * Punica/
 * ├── DS/  GBA/  3DS/  Switch/  GC-Wii/
 * │   ├── Games/    the games (one file each; a game made of several files keeps a folder)
 * │   ├── Saves/    the saves
 * │   ├── States/   the save states (and their screenshots)
 * │   ├── System/   BIOS, keys, firmware, the console's system memory
 * │   ├── Mods/     texture packs and game mods
 * │   └── Core/     the emulator's own files: settings, caches, logs, traces
 * ├── Settings/     Punica's settings, read back after a reinstall
 * └── Backups/      copies made before data is moved or imported over
 * ```
 *
 * What each core keeps where (checked in their code, 15 October):
 * - DS (and DSi): Punica's own code. System/ holds the DS BIOS set, System/DSi/ the DSi one (same file names).
 *   States/Previews/ holds the state screenshots (one folder per game, as SaveStateScreenshotProvider names them).
 * - GBA: Punica's own code.
 * - 3DS (Azahar): Azahar's folder is 3DS/ itself, its sub-folders pointed into the six (nand, sdmc and the keys
 *   in System, states in States, load in Mods, the rest in Core; azahar/src/common/file_util.cpp). A 3DS game's
 *   saves are on the emulated SD card (System/sdmc), mixed with installed titles: Saves/ only says so.
 * - Switch (Eden): Eden's folder is Switch/Core; its NAND and SD card in System, its saves folder (Eden's
 *   save_directory) in Saves, its mods folder in Mods (SwitchFolder).
 * - GameCube / Wii (Dolphin): Dolphin's folder is GC-Wii/Core; the Wii's NAND in System/Wii, the GameCube memory
 *   cards in Saves/GameCube, states in States, Load in Mods (dolphin/Source/Core/UICommon/UICommon.cpp). A Wii
 *   game's saves are in the Wii's NAND: Saves/ only says so.
 *
 * [migrate] moves an earlier layout (Roms/, BIOS/, Textures/, the cores' folders as they were) into this one.
 * Moves are renames on one storage; a name already taken at the target is never overwritten (the file goes to
 * Backups/Conflicts/). Plain java.io, background thread.
 */
object PunicaLayout {

    const val DS = "DS"
    const val GBA = "GBA"
    const val N3DS = "3DS"
    const val SWITCH = "Switch"
    const val GC_WII = "GC-Wii"
    val CONSOLES = listOf(DS, GBA, N3DS, SWITCH, GC_WII)

    const val GAMES = "Games"
    const val SAVES = "Saves"
    const val STATES = "States"
    const val SYSTEM = "System"
    const val MODS = "Mods"
    const val CORE = "Core"
    val PARTS = listOf(GAMES, SAVES, STATES, SYSTEM, MODS, CORE)

    const val SETTINGS = "Settings"
    const val BACKUPS = "Backups"
    /** DS state screenshots, in DS/States. */
    const val PREVIEWS = "Previews"
    /** The DSi BIOS set, in DS/System (its files have the DS set's names). */
    const val DSI = "DSi"
    /** The Switch firmware's NCAs, in Switch/System. */
    const val FIRMWARE = "Firmware"
    /** Dolphin's GameCube memory cards, in GC-Wii/Saves. */
    const val GAMECUBE = "GameCube"

    // written once the folder has this layout; its content is the layout's version
    private const val MARKER = ".punica-layout"
    private const val VERSION = "1"

    fun console(platform: RomPlatform): String = when (platform) {
        RomPlatform.NDS -> DS
        RomPlatform.GBA -> GBA
        RomPlatform.N3DS -> N3DS
        RomPlatform.SWITCH -> SWITCH
        RomPlatform.GC_WII -> GC_WII
    }

    // the DS game files the game list reads (Api24RomFileProcessorFactory): zip and 7z hold a DS game
    private val DS_EXTENSIONS = setOf("nds", "dsi", "ids", "zip", "7z")

    /** The console of a game file, by its extension as the game list reads it; null for any other file. */
    fun consoleOfFile(name: String): String? {
        val extension = name.substringAfterLast('.', "").lowercase()
        return when (extension) {
            in DS_EXTENSIONS -> DS
            in RomPlatform.gbaExtensions -> GBA
            in RomPlatform.n3dsExtensions -> N3DS
            in RomPlatform.switchExtensions -> SWITCH
            in RomPlatform.gcWiiExtensions -> GC_WII
            else -> null
        }
    }

    /**
     * Where [migrate] put a game file that was at [old] in the earlier Roms ([root]/Roms/<file> or
     * [root]/Roms/<folder>/<file>), or null when it is not there (or [old] was not in Roms). For entries of the game
     * list saved before the move (settings restored after a reinstall).
     */
    fun movedGame(root: File, old: File): File? {
        val relative = old.relativeToOrNull(File(root, "Roms"))?.invariantSeparatorsPath ?: return null
        if (relative.startsWith("..")) return null
        val console = consoleOfFile(old.name) ?: return null
        val games = part(root, console, GAMES)
        val parts = relative.split('/')
        return listOfNotNull(
            File(games, old.name),
            // a game of several files kept its folder
            if (parts.size > 1) File(games, relative) else null,
        ).firstOrNull { it.isFile }
    }

    fun part(root: File, console: String, part: String): File = File(File(root, console), part)

    fun isMigrated(root: File): Boolean = File(root, MARKER).isFile

    /** What [migrate] did: each game file's old and new place (for the game list), and what it left. */
    data class Report(val games: Map<File, File>, val moved: Int, val left: List<String>)

    /**
     * Moves [root]'s earlier layout into this one, then creates every console's six folders. [savesNotes] gives,
     * for the consoles whose saves cannot be apart (3DS, GC-Wii), the text of the note put in their Saves (file
     * name to text). Safe to run again: what is already in place is left.
     */
    fun migrate(root: File, savesNotes: Map<String, Pair<String, String>> = emptyMap()): Report {
        val run = Run(root)
        run.backupSaves()
        // BIOS first: a firmware already in BIOS/Switch/Firmware is then found by the scattered NCAs' check
        run.bios()
        run.firmwareFromRoms()
        run.games()
        run.oldConsoleFolders()
        run.topLevel()
        CONSOLES.forEach { console -> PARTS.forEach { part(root, console, it).mkdirs() } }
        savesNotes.forEach { (console, note) -> runCatching { File(part(root, console, SAVES), note.first).writeText(note.second) } }
        listOf(SETTINGS, BACKUPS).forEach { File(root, it).mkdirs() }
        File(root, MARKER).writeText(VERSION)
        return Report(run.games, run.moved, run.left)
    }

    private class Run(val root: File) {
        val games = LinkedHashMap<File, File>()
        var moved = 0
        val left = mutableListOf<String>()
        val stamp: String = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
        val conflicts = File(File(root, BACKUPS), "Conflicts-$stamp")
        // files of the earlier Roms that are no game: kept, out of the way
        val unsorted = File(File(root, BACKUPS), "Unsorted-$stamp")
        val saveFile = Regex(".*\\.sav$", RegexOption.IGNORE_CASE)
        val stateFile = Regex(".*\\.(ml\\d+|ss\\d+|ss\\d+\\.png)$", RegexOption.IGNORE_CASE)

        fun to(console: String, part: String, vararg names: String): File =
            names.fold(part(root, console, part)) { dir, name -> File(dir, name) }

        /**
         * Moves [from] to [to]. A folder whose target exists is merged into it, entry by entry; a file whose target
         * exists goes to Backups/Conflicts-<date>/ under its path in the folder instead (never overwritten).
         */
        fun move(from: File, to: File) {
            if (!from.exists()) return
            if (from.canonicalPath == to.canonicalPath) return
            if (to.exists()) {
                if (from.isDirectory && to.isDirectory) {
                    from.listFiles()?.forEach { move(it, File(to, it.name)) }
                    from.delete()
                    return
                }
                // the same file already there (a copy a core had made of a BIOS file): nothing to keep twice
                if (from.isFile && to.isFile && sameContent(from, to)) {
                    if (from.delete()) moved++ else left += from.path
                    return
                }
                val aside = File(conflicts, from.relativeTo(root).path)
                if (aside.exists()) { left += from.path; return }
                rename(from, aside)
                return
            }
            rename(from, to)
        }

        private fun rename(from: File, to: File) {
            to.parentFile?.mkdirs()
            if (from.renameTo(to)) { moved++; return }
            // another storage: copied whole, checked, then the original removed
            val copied = runCatching { from.copyRecursively(to, overwrite = false) }.getOrDefault(false) && sameSize(from, to)
            if (copied && from.deleteRecursively()) moved++ else left += from.path
        }

        private fun sameContent(a: File, b: File): Boolean {
            if (a.length() != b.length()) return false
            a.inputStream().buffered().use { x ->
                b.inputStream().buffered().use { y ->
                    while (true) {
                        val c = x.read()
                        if (c != y.read()) return false
                        if (c == -1) return true
                    }
                }
            }
        }

        private fun sameSize(a: File, b: File): Boolean =
            a.walkTopDown().filter { it.isFile }.sumOf { it.length() } == b.walkTopDown().filter { it.isFile }.sumOf { it.length() }

        /** Each entry of [dir] into [target] (the folder's content, not the folder), then [dir] removed if emptied. */
        fun moveContent(dir: File, target: File) {
            dir.listFiles()?.forEach { move(it, File(target, it.name)) }
            dir.delete()
        }

        /**
         * The saves, before anything moves: the DS and GBA saves and states, the Switch saves and the GameCube memory
         * cards, copied into Backups/Saves-<date>/. The 3DS SD card and the Wii NAND (games and saves mixed, several
         * gigabytes) are not copied: each moves as one folder, by a single rename.
         */
        fun backupSaves() {
            if (isMigrated(root)) return
            val backup = File(File(root, BACKUPS), "Saves-$stamp")
            val backedUp = Regex(".*\\.(sav|ml\\d+|ss\\d+|ss\\d+\\.png)$", RegexOption.IGNORE_CASE)
            listOf(DS, GBA).forEach { console ->
                File(root, console).listFiles { f -> f.isFile && backedUp.matches(f.name) }?.forEach { f ->
                    runCatching { f.copyTo(File(File(backup, console), f.name)) }.onFailure { left += "backup: ${f.path}" }
                }
            }
            listOf(File(root, "$SWITCH/nand/user/save") to "$SWITCH/save", File(root, "$GC_WII/GC") to "$GC_WII/GC").forEach { (dir, name) ->
                if (dir.isDirectory) runCatching { dir.copyRecursively(File(backup, name)) }.onFailure { left += "backup: ${dir.path}" }
            }
        }

        /** A Switch firmware's NCAs the old folder organizer had put one by one in Roms/<hash>/ (14 October). */
        fun firmwareFromRoms() {
            File(root, "Roms").listFiles { f -> f.isDirectory }?.forEach { dir ->
                val nca = dir.listFiles()?.singleOrNull()?.takeIf { it.isFile && RomPlatform.isSwitchContentFile(it.name) } ?: return@forEach
                val target = to(SWITCH, SYSTEM, FIRMWARE, nca.name)
                // named after its content's hash: the same name and size is the same file
                if (target.isFile && target.length() == nca.length()) { if (nca.delete()) dir.delete() } else { move(nca, target); dir.delete() }
            }
        }

        /** Roms/ into each console's Games: one game file each; a game folder with several files kept as a folder. */
        fun games() {
            val roms = File(root, "Roms")
            roms.listFiles()?.forEach { entry ->
                if (entry.name.startsWith(".")) return@forEach
                if (entry.isDirectory) sortSavesBesideGame(entry)
                if (entry.isFile) {
                    val console = consoleOfFile(entry.name) ?: run { move(entry, File(unsorted, entry.name)); return@forEach }
                    moveGame(entry, to(console, GAMES, entry.name))
                    return@forEach
                }
                val content = entry.listFiles()?.filterNot { it.name.startsWith(".") }.orEmpty()
                when {
                    content.isEmpty() -> entry.listFiles()?.forEach { it.delete() }.also { entry.delete() }
                    content.size == 1 && content[0].isFile -> {
                        val file = content[0]
                        val console = consoleOfFile(file.name) ?: run { move(entry, File(unsorted, entry.name)); return@forEach }
                        moveGame(file, to(console, GAMES, file.name))
                        entry.listFiles()?.forEach { if (it.name.startsWith(".")) it.delete() }
                        entry.delete()
                    }
                    else -> {
                        // the console of its first game file; its files keep their places inside it
                        val console = content.firstNotNullOfOrNull { if (it.isFile) consoleOfFile(it.name) else null }
                            ?: run { move(entry, File(unsorted, entry.name)); return@forEach }
                        val target = to(console, GAMES, entry.name)
                        val files = entry.walkTopDown().filter { it.isFile }.toList()
                        if (target.exists()) { left += entry.path; return@forEach }
                        move(entry, target)
                        if (target.isDirectory) files.forEach { games[it] = File(target, it.relativeTo(entry).path) }
                    }
                }
            }
            roms.listFiles()?.filter { it.name.startsWith(".") }?.forEach { it.delete() }
            if (!roms.delete() && roms.exists()) left += roms.path
        }

        // a save or state kept next to its game (GBA did, before 13 October): into its console's Saves or States
        private fun sortSavesBesideGame(dir: File) {
            val console = dir.listFiles()?.firstNotNullOfOrNull { if (it.isFile) consoleOfFile(it.name) else null } ?: return
            dir.listFiles()?.filter { it.isFile }?.forEach { file ->
                when {
                    saveFile.matches(file.name) -> move(file, to(console, SAVES, file.name))
                    stateFile.matches(file.name) -> move(file, to(console, STATES, file.name))
                }
            }
        }

        private fun moveGame(file: File, target: File) {
            val before = moved
            move(file, target)
            if (moved > before && target.isFile) games[file] = target
        }

        /** BIOS/<console>/ into each console's System. */
        fun bios() {
            val bios = File(root, "BIOS")
            moveContent(File(bios, "DS"), to(DS, SYSTEM))
            moveContent(File(bios, "DSi"), to(DS, SYSTEM, DSI))
            moveContent(File(bios, "GBA"), to(GBA, SYSTEM))
            moveContent(File(bios, "3DS"), to(N3DS, SYSTEM))
            moveContent(File(bios, "Switch"), to(SWITCH, SYSTEM))
            moveContent(File(bios, "GC-Wii"), to(GC_WII, SYSTEM))
            if (!bios.delete() && bios.exists()) left += bios.path
        }

        /** What the console folders held before: sorted into the six folders of each. */
        fun oldConsoleFolders() {
            sortSaves(DS, Regex(".*\\.sav$", RegexOption.IGNORE_CASE), Regex(".*\\.ml\\d+$", RegexOption.IGNORE_CASE))
            sortSaves(GBA, Regex(".*\\.sav$", RegexOption.IGNORE_CASE), Regex(".*\\.ss\\d+(\\.png)?$", RegexOption.IGNORE_CASE))

            // 3DS: Azahar's folder, which stays 3DS/ itself (its sub-folders are pointed into the six)
            val n3ds = File(root, N3DS)
            sortCore(n3ds) { name, entry ->
                when (name) {
                    "nand", "sdmc" -> move(entry, to(N3DS, SYSTEM, name))
                    "sysdata" -> moveContent(entry, to(N3DS, SYSTEM))
                    "states" -> moveContent(entry, to(N3DS, STATES))
                    "load" -> moveContent(entry, to(N3DS, MODS))
                    else -> move(entry, to(N3DS, CORE, name))
                }
            }

            // Switch: Eden's whole folder becomes Switch/Core; its NAND and SD card go to System, its saves to Saves
            val switch = File(root, SWITCH)
            sortCore(switch) { name, entry ->
                when (name) {
                    "nand" -> {
                        File(entry, "user/save").takeIf { it.isDirectory }?.let { move(it, to(SWITCH, SAVES, "user", "save")) }
                        move(entry, to(SWITCH, SYSTEM, "nand"))
                    }
                    "sdmc" -> move(entry, to(SWITCH, SYSTEM, "sdmc"))
                    "load" -> moveContent(entry, to(SWITCH, MODS))
                    else -> move(entry, to(SWITCH, CORE, name))
                }
            }

            // GameCube / Wii: Dolphin's whole folder becomes GC-Wii/Core; the Wii NAND to System, memory cards to Saves
            val gcWii = File(root, GC_WII)
            sortCore(gcWii) { name, entry ->
                when (name) {
                    "Wii" -> move(entry, to(GC_WII, SYSTEM, "Wii"))
                    "StateSaves" -> moveContent(entry, to(GC_WII, STATES))
                    "Load" -> moveContent(entry, to(GC_WII, MODS))
                    else -> move(entry, to(GC_WII, CORE, name))
                }
            }
            // memory cards: GC/MemoryCardA.USA.raw and GC/<region>/Card A, as Dolphin's custom paths take them
            val gc = to(GC_WII, CORE, "GC")
            gc.listFiles { f -> f.isFile && f.name.endsWith(".raw", true) }?.forEach { move(it, to(GC_WII, SAVES, GAMECUBE, it.name)) }
            gc.listFiles { f -> f.isDirectory }?.forEach { region ->
                region.listFiles { f -> f.isDirectory && f.name.startsWith("Card ") }?.forEach { card ->
                    move(card, to(GC_WII, SAVES, GAMECUBE, card.name, region.name))
                }
            }
        }

        // the DS and GBA folders held their saves and states loose, and the DS debug traces
        private fun sortSaves(console: String, save: Regex, state: Regex) {
            File(root, console).listFiles()?.forEach { entry ->
                if (entry.name in PARTS || entry.name.startsWith(".")) return@forEach
                when {
                    entry.isFile && save.matches(entry.name) -> move(entry, to(console, SAVES, entry.name))
                    entry.isFile && state.matches(entry.name) -> move(entry, to(console, STATES, entry.name))
                    else -> move(entry, to(console, CORE, entry.name))
                }
            }
        }

        // every entry of a core's folder that is not one of the six, given to [sort]
        private fun sortCore(dir: File, sort: (String, File) -> Unit) {
            dir.listFiles()?.forEach { entry ->
                if (entry.name in PARTS || entry.name.startsWith(".")) return@forEach
                sort(entry.name, entry)
            }
        }

        /** The other top-level folders of the earlier layout. */
        fun topLevel() {
            moveContent(File(root, "Textures"), to(DS, MODS))
            moveContent(File(root, "Save state previews"), to(DS, STATES, PREVIEWS))
            listOf("Inspector", "Materials").forEach { move(File(root, it), to(DS, CORE, it)) }
        }
    }
}
