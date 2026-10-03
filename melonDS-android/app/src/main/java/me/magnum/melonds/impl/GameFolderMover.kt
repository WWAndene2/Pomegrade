package me.magnum.melonds.impl

import java.io.File

/**
 * Moves a game into its own folder (see [PomegradeFolder]): the ROM, and the files that go with
 * it. Files only (java.io), no Android API, so it can be tested on its own.
 */
object GameFolderMover {

    /**
     * Moves [rom] into a folder of [romsDir] named after its file ("Game (2)" if a file of that
     * name is already in "Game"), with each of [companions] that exists. Returns the folder's name,
     * or null if the ROM couldn't be moved (it stays where it was). A companion that can't follow
     * stays where it was.
     */
    fun move(rom: File, companions: List<File>, romsDir: File): String? {
        if (!rom.isFile) return null
        val folderBase = folderNameFor(rom)
        var folderName = folderBase
        var n = 2
        while (File(File(romsDir, folderName), rom.name).exists()) {
            folderName = "$folderBase ($n)"
            n++
        }
        val folder = File(romsDir, folderName)
        if (!folder.isDirectory && !folder.mkdirs()) return null

        if (!moveFile(rom, File(folder, rom.name))) {
            folder.delete() // only if it was left empty
            return null
        }
        for (file in companions.distinct()) {
            if (file.isFile && file.parentFile?.canonicalFile != folder.canonicalFile) {
                moveFile(file, File(folder, file.name))
            }
        }
        return folderName
    }

    /** The ROM's file name without its extension, without the characters a folder name can't have. */
    fun folderNameFor(rom: File): String =
        rom.nameWithoutExtension.replace(Regex("[\\\\/:*?\"<>|]"), "_").trim().ifEmpty { "Game" }

    /**
     * A rename on the same storage volume; between volumes (an SD card and the internal storage), a
     * copy checked by size, then the original deleted. Never replaces an existing file.
     */
    fun moveFile(from: File, to: File): Boolean {
        if (to.exists()) return false
        if (from.renameTo(to)) return true
        return try {
            from.inputStream().use { input -> to.outputStream().use { output -> input.copyTo(output) } }
            if (to.length() == from.length()) {
                from.delete()
                true
            } else {
                to.delete()
                false
            }
        } catch (e: Exception) {
            to.delete()
            false
        }
    }
}
