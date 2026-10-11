package me.magnum.melonds.impl

import android.content.Context
import java.io.File

/**
 * Where the Dolphin core keeps its data (GameCube memory cards, Wii NAND and saves, settings, textures):
 * the Pomegrade folder's GC-Wii, as the 3DS and Switch cores' data are in its 3DS and Switch, so it stays
 * when the app is uninstalled. Without the Pomegrade folder's file access, the app's own folder's dolphin:
 * Dolphin's default, the app's own folder itself, is the Switch core's folder too.
 */
object GcWiiFolder {

    private const val APP_FOLDER = "dolphin"

    /** Given to Dolphin (DirectoryInitialization.userDirectoryProvider), read when it starts. */
    fun userDirectory(context: Context): File? {
        val folder = PomegradeFolder.subFolder(context, PomegradeFolder.GC_WII) ?: return context.getExternalFilesDir(APP_FOLDER)
        return folder.takeIf { it.isDirectory || it.mkdirs() } ?: context.getExternalFilesDir(APP_FOLDER)
    }
}
