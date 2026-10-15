package me.magnum.melonds.impl

import android.content.Context
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.io.File

/**
 * The Switch core's data in the Punica folder's Switch, in the layout of [PunicaLayout]:
 *
 * ```
 * Punica/Switch/
 * ├── Core/          Eden's own folder (its user folder: settings, keys, logs, shader caches...)
 * ├── System/nand/   the Switch's internal memory: installed updates and DLC, the firmware
 * ├── System/sdmc/   its SD card
 * ├── Saves/         the game saves (Eden's saves folder, user/save/...)
 * └── Mods/          the game mods (Eden's load folder, <title id>/<mod>/)
 * ```
 *
 * Eden keeps all of it in the app's own folder (Android/data), which Android deletes with the app; here it stays,
 * with the other consoles'. Eden's user folder moves once ([moveUserData]); its NAND, SD card, saves and mods
 * folders are set at every Switch launch ([setUp]), since Eden ignores a folder its settings name that does not
 * exist. Before the Punica folder has the layout, Punica/Switch itself is Eden's user folder, as it was. Needs
 * Eden started (YuzuApplication.start, on the main thread); runs on a background thread.
 */
object SwitchFolder {

    private const val NAND = "nand"
    private const val SDMC = "sdmc"
    private const val LOAD = "load"
    // Eden's user folder entries other than nand, sdmc and load (common/fs/fs_paths.h)
    private val USER_DATA = listOf("config", "keys", "log", "shader", "dump", "screenshots", "amiibo", "play_time", "icons", "crash_dumps", "tas", "cache")
    private const val USER_DATA_MOVED = "switch_user_data_moved"

    /**
     * Given to Eden (DirectoryInitialization.userDirectoryProvider): Punica/Switch/Core once the Punica folder has
     * the layout, else Punica/Switch once Eden's data has moved there, or null (Eden's own folder) before that or
     * without the folder's file access.
     */
    fun userDirectory(context: Context): File? {
        PomegradeFolder.layoutRoot(context)?.let { root ->
            return PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.CORE).takeIf { it.isDirectory || it.mkdirs() }
        }
        if (!PreferenceManager.getDefaultSharedPreferences(context).getBoolean(USER_DATA_MOVED, false)) return null
        return PomegradeFolder.subFolder(context, PomegradeFolder.SWITCH)?.takeIf { it.isDirectory || it.mkdirs() }
    }

    /**
     * Moves Eden's data from the app's own folder (Android/data, deleted with the app) into its user folder in the
     * Punica folder, once, before Eden starts (it reads its folder when it starts). An entry already there (after a
     * reinstall) is kept and Eden's copy left where it is. Background thread.
     */
    fun moveUserData(context: Context) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        if (preferences.getBoolean(USER_DATA_MOVED, false)) return
        val target = (PomegradeFolder.layoutRoot(context)?.let { PunicaLayout.part(it, PunicaLayout.SWITCH, PunicaLayout.CORE) }
            ?: PomegradeFolder.subFolder(context, PomegradeFolder.SWITCH))?.takeIf { it.isDirectory || it.mkdirs() } ?: return
        val source = context.getExternalFilesDir(null) ?: return
        val moved = USER_DATA.all { name ->
            val from = File(source, name)
            val to = File(target, name)
            !from.exists() || to.exists() || from.renameTo(to) || (from.copyRecursively(to) && from.deleteRecursively())
        }
        // tried again at the next start if a copy failed: nothing is lost, the rest stays in Eden's folder
        if (moved) preferences.edit().putBoolean(USER_DATA_MOVED, true).apply()
    }

    /**
     * Points Eden's NAND, SD card, saves and mods folders to Switch/System/nand, Switch/System/sdmc, Switch/Saves and
     * Switch/Mods. What Eden keeps at its own default places (in its user folder, or the saves inside the NAND) moves
     * there first; a folder the owner picked in Eden's own settings, elsewhere, is left alone.
     */
    fun setUp(context: Context) {
        val root = PomegradeFolder.layoutRoot(context) ?: return
        val userDirectory = File(DirectoryInitialization.userDirectory ?: return).canonicalFile
        val nand = File(PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.SYSTEM), NAND)
        val sdmc = File(PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.SYSTEM), SDMC)
        val saves = PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.SAVES)
        val mods = PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.MODS)
        val ours = listOf(nand, sdmc, saves, mods).map { it.canonicalFile }

        // a folder Eden uses now: its default (in its user folder) moves to ours; one of ours is fine; any other is the
        // owner's choice, kept
        fun adopt(current: String, target: File): Boolean {
            val currentDir = File(current).canonicalFile
            if (currentDir == target.canonicalFile) return true
            if (currentDir in ours) return true
            if (!currentDir.path.startsWith(userDirectory.path + File.separator)) return false
            moveIfAbsent(currentDir, target)
            return true
        }

        val currentNand = File(NativeConfig.getNandDir()).canonicalFile
        // the saves follow the NAND unless set apart (Eden's default is the NAND itself, saves in user/save)
        if (File(NativeConfig.getSaveDir()).canonicalFile == currentNand) {
            moveIfAbsent(File(currentNand, "user/save"), File(saves, "user/save"))
            saves.mkdirs()
            NativeConfig.setSaveDir(saves.absolutePath)
        } else if (adopt(NativeConfig.getSaveDir(), saves)) {
            saves.mkdirs()
            NativeConfig.setSaveDir(saves.absolutePath)
        }
        if (adopt(currentNand.path, nand)) { nand.mkdirs(); NativeConfig.setNandDir(nand.absolutePath) }
        if (adopt(NativeConfig.getSdmcDir(), sdmc)) { sdmc.mkdirs(); NativeConfig.setSdmcDir(sdmc.absolutePath) }
        if (adopt(NativeConfig.getLoadDir(), mods)) { mods.mkdirs(); NativeConfig.setLoadDir(mods.absolutePath) }
        NativeConfig.saveGlobalConfig()
        NativeLibrary.initializeSystem(true)
    }

    /**
     * Where a path of Eden's user folder ("nand/user/save/...", "load/...", "config/config.ini") goes in [switch] (the
     * Punica folder's Switch): with the layout, its saves in Saves, NAND and SD card in System, mods in Mods, the
     * rest in Core; without it, [switch] is Eden's user folder itself.
     */
    fun placeOf(switch: File, edenPath: String, layout: Boolean): File {
        if (!layout) return File(switch, edenPath)
        fun under(part: String, path: String) = File(File(switch, part), path)
        return when {
            edenPath == "nand/user/save" || edenPath.startsWith("nand/user/save/") -> under(PunicaLayout.SAVES, edenPath.removePrefix("nand/"))
            edenPath == NAND || edenPath.startsWith("$NAND/") || edenPath == SDMC || edenPath.startsWith("$SDMC/") -> under(PunicaLayout.SYSTEM, edenPath)
            edenPath == LOAD || edenPath.startsWith("$LOAD/") -> under(PunicaLayout.MODS, edenPath.removePrefix(LOAD).trimStart('/'))
            else -> under(PunicaLayout.CORE, edenPath)
        }
    }

    // [source] into [target] when [target] does not exist yet; an empty [source] left by Eden beside an existing
    // [target] is removed, a non-empty one kept (nothing is overwritten)
    private fun moveIfAbsent(source: File, target: File) {
        if (!source.exists()) return
        if (target.exists()) {
            if (source.isDirectory && source.walkTopDown().none { it.isFile }) source.deleteRecursively()
            return
        }
        target.parentFile?.mkdirs()
        if (!source.renameTo(target)) {
            if (source.copyRecursively(target)) source.deleteRecursively()
        }
    }
}
