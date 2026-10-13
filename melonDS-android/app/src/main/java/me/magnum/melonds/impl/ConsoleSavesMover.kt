package me.magnum.melonds.impl

import java.io.File

/**
 * Moves the DS and GBA saves kept next to their games (Roms/<game>/, as Punica did before 13 October) into
 * their console's folder (DS/, GBA/), where the 3DS, Switch and GameCube / Wii saves already are (owner's
 * decision: each console's saves in its own folder). A game folder's console is told by its ROM's extension;
 * a file the console folder already has is left where it is, never replaced. Files only (java.io), no
 * Android API, so it can be tested on its own.
 */
object ConsoleSavesMover {

    enum class Console { DS, GBA }

    private val DS_ROMS = setOf("nds", "dsi", "srl")
    private val GBA_ROMS = setOf("gba")
    // DS: the battery save, the quick slot .ml0 and slots .ml1 to .ml10 (FileSystemSaveStatesRepository)
    private val DS_SAVE = Regex(".+\\.(sav|ml([0-9]|10))", RegexOption.IGNORE_CASE)
    // GBA: the battery save, the states .ss0 to .ss9 and their previews (GbaEmulatorActivity)
    private val GBA_SAVE = Regex(".+\\.(sav|ss[0-9](\\.png)?)", RegexOption.IGNORE_CASE)

    /** The console of the ROM in [gameDir], or null when it holds no DS or GBA ROM. */
    fun consoleOf(gameDir: File): Console? {
        val extensions = gameDir.listFiles()?.filter { it.isFile }?.map { it.extension.lowercase() }.orEmpty()
        return when {
            extensions.any { it in DS_ROMS } -> Console.DS
            extensions.any { it in GBA_ROMS } -> Console.GBA
            else -> null
        }
    }

    /** The save files of a [console] game kept in [gameDir]. */
    fun savesIn(gameDir: File, console: Console): List<File> {
        val pattern = if (console == Console.DS) DS_SAVE else GBA_SAVE
        return gameDir.listFiles()?.filter { it.isFile && pattern.matches(it.name) }.orEmpty()
    }

    /** Moves the saves of every game folder of [romsDir] into [dsDir] or [gbaDir]; returns how many moved. */
    fun moveAll(romsDir: File, dsDir: File, gbaDir: File): Int {
        var moved = 0
        romsDir.listFiles()?.filter { it.isDirectory }?.forEach { gameDir ->
            val console = consoleOf(gameDir) ?: return@forEach
            val target = if (console == Console.DS) dsDir else gbaDir
            if (!target.isDirectory && !target.mkdirs()) return@forEach
            savesIn(gameDir, console).forEach { save ->
                if (GameFolderMover.moveFile(save, File(target, save.name))) moved++
            }
        }
        return moved
    }
}
