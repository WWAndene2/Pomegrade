package me.magnum.melonds.impl

import android.content.Context
import java.io.File

/**
 * Where the Dolphin core keeps its data: the Punica folder's GC-Wii/Core, Dolphin's user folder (settings, caches,
 * logs, the GameCube boot ROMs it reads), with the console's other folders beside it in the layout of
 * [PunicaLayout]: Dolphin itself points the Wii NAND to GC-Wii/System/Wii, the memory cards to GC-Wii/Saves/GameCube,
 * the states to GC-Wii/States and Load to GC-Wii/Mods (UsePunicaLayout, dolphin/Source/Core/UICommon/UICommon.cpp).
 * Before the Punica folder has the layout, GC-Wii itself, as it was. Without the folder's file access, the app's own
 * folder's dolphin: Dolphin's default, the app's own folder itself, is the Switch core's folder too.
 */
object GcWiiFolder {

    private const val APP_FOLDER = "dolphin"

    /** Given to Dolphin (DirectoryInitialization.userDirectoryProvider), read when it starts. */
    fun userDirectory(context: Context): File? {
        val folder = PomegradeFolder.layoutRoot(context)?.let { PunicaLayout.part(it, PunicaLayout.GC_WII, PunicaLayout.CORE) }
            ?: PomegradeFolder.subFolder(context, PomegradeFolder.GC_WII)
            ?: return context.getExternalFilesDir(APP_FOLDER)
        return folder.takeIf { it.isDirectory || it.mkdirs() } ?: context.getExternalFilesDir(APP_FOLDER)
    }

    /**
     * To call after the Pomegrade folder was migrated to the layout, renamed or changed: Dolphin resolves its user
     * folder once per process, this makes it resolve [userDirectory] again and re-apply its paths. Does nothing
     * before Dolphin has started (it reads the folder when it starts). While a GameCube / Wii game runs, nothing
     * changes and false is returned: the game keeps its folders, the new ones apply at the next call or app start.
     */
    fun onUserFolderChanged(context: Context): Boolean =
        org.dolphinemu.dolphinemu.utils.DirectoryInitialization.reapplyUserDirectory(context.applicationContext)
}
