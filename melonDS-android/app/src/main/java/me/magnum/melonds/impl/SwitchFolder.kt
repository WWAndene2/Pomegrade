package me.magnum.melonds.impl

import android.content.Context
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.io.File

/**
 * The Switch core's data in the Pomegrade folder's Switch, as the 3DS core's is in its 3DS:
 *
 * ```
 * Pomegrade/Switch/
 * ├── nand/   the Switch's internal memory: game saves, installed updates and DLC, the firmware
 * └── sdmc/   its SD card: mods, screenshots
 * ```
 *
 * Eden keeps them in the app's own folder (Android/data), which Android deletes with the app; here they
 * stay, with the other consoles' saves. The first time, what Eden already has there is moved; if
 * Pomegrade/Switch already has data (after a reinstall), that data is used and Eden's is left. Folders
 * the owner picked in Eden's own settings are left alone. Needs Eden started (YuzuApplication.start, on
 * the main thread); runs on a background thread.
 *
 * Eden's other data (its settings, keys, logs, shader caches, mods, screenshots...) moves into the same
 * folder once ([moveUserData]): Pomegrade/Switch becomes Eden's whole user folder ([userDirectory]), so
 * nand and sdmc are where Eden looks by default.
 */
object SwitchFolder {

    private const val NAND = "nand"
    private const val SDMC = "sdmc"
    // Eden's user folder entries other than nand and sdmc (common/fs/fs_paths.h)
    private val USER_DATA = listOf("config", "keys", "log", "shader", "load", "dump", "screenshots", "amiibo", "play_time", "icons", "crash_dumps", "tas", "cache")
    private const val USER_DATA_MOVED = "switch_user_data_moved"

    /**
     * Given to Eden (DirectoryInitialization.userDirectoryProvider): Pomegrade/Switch once Eden's data has
     * moved there, or null (Eden's own folder) before that or without the Pomegrade folder's file access.
     */
    fun userDirectory(context: Context): File? {
        if (!PreferenceManager.getDefaultSharedPreferences(context).getBoolean(USER_DATA_MOVED, false)) return null
        return PomegradeFolder.subFolder(context, PomegradeFolder.SWITCH)?.takeIf { it.isDirectory || it.mkdirs() }
    }

    /**
     * Moves Eden's data from the app's own folder (Android/data, deleted with the app) into Pomegrade/Switch,
     * once, before Eden starts (it reads its folder when it starts). An entry already in Pomegrade/Switch
     * (after a reinstall) is kept and Eden's copy left where it is. Background thread.
     */
    fun moveUserData(context: Context) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        if (preferences.getBoolean(USER_DATA_MOVED, false)) return
        val target = PomegradeFolder.subFolder(context, PomegradeFolder.SWITCH)?.takeIf { it.isDirectory || it.mkdirs() } ?: return
        val source = context.getExternalFilesDir(null) ?: return
        val moved = USER_DATA.all { name ->
            val from = File(source, name)
            val to = File(target, name)
            !from.exists() || to.exists() || from.renameTo(to) || (from.copyRecursively(to) && from.deleteRecursively())
        }
        // tried again at the next start if a copy failed: nothing is lost, the rest stays in Eden's folder
        if (moved) preferences.edit().putBoolean(USER_DATA_MOVED, true).apply()
    }

    fun setUp(context: Context) {
        val root = PomegradeFolder.subFolder(context, PomegradeFolder.SWITCH) ?: return
        val userDirectory = File(DirectoryInitialization.userDirectory ?: return)
        val nand = File(root, NAND)
        val sdmc = File(root, SDMC)
        val currentNand = File(NativeConfig.getNandDir()).canonicalFile
        if (currentNand == nand.canonicalFile) return
        if (currentNand != File(userDirectory, NAND).canonicalFile) return // the owner's own choice in Eden

        moveIfAbsent(File(userDirectory, NAND), nand)
        if (File(NativeConfig.getSdmcDir()).canonicalFile == File(userDirectory, SDMC).canonicalFile) {
            moveIfAbsent(File(userDirectory, SDMC), sdmc)
            NativeConfig.setSdmcDir(sdmc.absolutePath)
        }
        // the saves folder follows the NAND unless set apart (Eden's default is the NAND itself)
        if (File(NativeConfig.getSaveDir()).canonicalFile == currentNand) NativeConfig.setSaveDir(nand.absolutePath)
        NativeConfig.setNandDir(nand.absolutePath)
        NativeConfig.saveGlobalConfig()
        NativeLibrary.initializeSystem(true)
    }

    private fun moveIfAbsent(source: File, target: File) {
        if (target.exists()) {
            target.mkdirs()
            return
        }
        target.parentFile?.mkdirs()
        if (!source.exists()) {
            target.mkdirs()
        } else if (!source.renameTo(target)) {
            source.copyRecursively(target)
            source.deleteRecursively()
        }
    }
}
