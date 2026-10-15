package me.magnum.melonds.impl

import java.io.File

/**
 * Moves a game file (see [PomegradeFolder]): files only (java.io), no Android API, so it can be tested on its own.
 */
object GameFolderMover {

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
