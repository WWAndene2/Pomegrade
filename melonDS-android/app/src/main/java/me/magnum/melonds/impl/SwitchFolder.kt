package me.magnum.melonds.impl

import android.content.Context
import android.util.Log
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
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
 *
 * Eden reads its user folder once per process; when the app's folder changes (a migration, the Punica rename, a
 * new folder), [onUserFolderChanged] points Eden to the new one, and [setUp] does it again at each launch for a
 * change it could not apply then (a game running).
 */
object SwitchFolder {

    private const val NAND = "nand"
    private const val SDMC = "sdmc"
    private const val LOAD = "load"
    // Eden's user folder entries other than nand, sdmc and load (common/fs/fs_paths.h)
    private val USER_DATA = listOf("config", "keys", "log", "shader", "dump", "screenshots", "amiibo", "play_time", "icons", "crash_dumps", "tas", "cache", "gpu_drivers")
    private const val USER_DATA_MOVED = "switch_user_data_moved"
    // the Punica folder's Switch that [setUp] last pointed Eden to: Eden's settings may still name folders in it
    // after the owner picks another folder, they are then adopted rather than kept as the owner's choice
    private const val LAST_SWITCH_FOLDER = "switch_last_folder"
    private const val TAG = "SwitchFolder"

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
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val wanted = userDirectory(context)?.canonicalFile
        if (wanted == null && preferences.getBoolean(USER_DATA_MOVED, false)) {
            // Eden runs in the app's own folder (DirectoryInitialization.userDirectoryFile's fallback), a fresh one:
            // told to the owner by SwitchLauncher.prepare; what is written there is merged back when access returns
            Log.w(TAG, "No access to the Punica folder: the Switch core uses the app's own folder")
            throw IllegalStateException("no access to the Punica folder, the Switch core uses the app's own folder until it returns")
        }
        val former = mutableSetOf<File>()
        preferences.getString(LAST_SWITCH_FOLDER, null)?.let { former += File(it) }
        realign(context, wanted)?.let { previous ->
            former += previous
            if (previous.name == PunicaLayout.CORE) previous.parentFile?.let { former += it }
        }
        mergeFallbackData(context)

        val root = PomegradeFolder.layoutRoot(context) ?: return
        val userDirectory = File(DirectoryInitialization.userDirectory ?: return).canonicalFile
        val nand = File(PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.SYSTEM), NAND)
        val sdmc = File(PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.SYSTEM), SDMC)
        val saves = PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.SAVES)
        val mods = PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.MODS)
        val ours = listOf(nand, sdmc, saves, mods).map { it.canonicalFile }

        val switch = File(root, PunicaLayout.SWITCH).canonicalFile
        val formerFolders = (former.map { it.canonicalFile } - switch).toList()
        val fallback = context.getExternalFilesDir(null)?.canonicalFile

        // a folder Eden uses now: its default (in its user folder), or one in the former Punica folder or the app's
        // own folder, moves to ours; one of ours is fine; one Eden cannot use (missing, unreadable, another app's
        // Android/data, as a config.ini from Eden's own app names) is replaced by ours; any other is the owner's
        // choice, kept
        fun adopt(current: String, target: File): Boolean {
            val currentDir = File(current).canonicalFile
            if (currentDir == target.canonicalFile) return true
            if (currentDir in ours) return true
            val movable = listOfNotNull(userDirectory, fallback).plus(formerFolders).any { isInside(currentDir, it) }
            if (movable) {
                moveIfAbsent(currentDir, target)
                return true
            }
            return !isOwnersChoice(currentDir, fallback)
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
        preferences.edit().putString(LAST_SWITCH_FOLDER, switch.path).apply()
    }

    /**
     * After the app's folder changed (a migration, the Punica folder's rename, another folder picked): Eden is
     * pointed to its user folder there ([userDirectory]), its NAND, SD card, saves and mods folders set to the new
     * layout's (the ones in the former folder adopted, see [setUp]), and the keys and firmware checked against the
     * new NAND ([SwitchSystemFiles]). Before Eden starts in this process there is nothing to do: it reads the folder
     * when it starts. While a game runs, Eden's open files stay in the former folder: its user folder is changed at
     * the next launch, when [setUp] runs again (SwitchLauncher.prepare); the game's saves are written to the former
     * folder until then, and moved with it only where the new folder has none (otherwise they stay there, never
     * overwriting); nothing is moved under a running game.
     * Throws as [setUp] does (no access to the new folder). Background thread.
     */
    fun onUserFolderChanged(context: Context) {
        if (!YuzuApplication.isStarted) return
        if (NativeLibrary.isRunning()) {
            Log.i(TAG, "Punica folder changed while a Switch game runs: the Switch core follows at the next launch")
            return
        }
        setUp(context)
        SwitchSystemFiles.import(context)
    }

    /**
     * Points Eden to [wanted] when it started in another user folder in this process (DirectoryInitialization reads
     * it once), unless a game runs. Returns the folder it used before, or null when unchanged.
     */
    private fun realign(context: Context, wanted: File?): File? {
        if (wanted == null) return null
        val current = DirectoryInitialization.userDirectory?.let { File(it).canonicalFile } ?: return null
        if (current == wanted) return null
        if (NativeLibrary.isRunning()) {
            Log.i(TAG, "Switch user folder changed to $wanted while a game runs: applied at the next launch")
            return null
        }
        if (!DirectoryInitialization.reinitialize(context)) return null
        Log.i(TAG, "Switch user folder changed from $current to $wanted")
        return current
    }

    /**
     * Brings into Eden's user folder what Eden wrote in the app's own folder (Android/data, deleted with the app)
     * while it had no access to the Punica folder (its fallback), once its data had moved: files the Punica folder
     * lacks move there, a file it already has is kept on both sides (never overwritten) and logged. Not while Eden
     * uses that folder.
     */
    private fun mergeFallbackData(context: Context) {
        if (!PreferenceManager.getDefaultSharedPreferences(context).getBoolean(USER_DATA_MOVED, false)) return
        val fallback = context.getExternalFilesDir(null)?.canonicalFile ?: return
        val live = DirectoryInitialization.userDirectory?.let { File(it).canonicalFile } ?: return
        if (isInside(live, fallback) || isInside(fallback, live)) return
        val switch = (PomegradeFolder.layoutRoot(context)?.let { File(it, PunicaLayout.SWITCH) }
            ?: PomegradeFolder.subFolder(context, PomegradeFolder.SWITCH)) ?: return
        val layout = PomegradeFolder.layoutRoot(context) != null
        var moved = 0
        var kept = 0
        (USER_DATA + listOf(NAND, SDMC, LOAD)).map { File(fallback, it) }.filter { it.exists() }.forEach { entry ->
            entry.walkTopDown().filter { it.isFile }.forEach { file ->
                val to = placeOf(switch, "${entry.name}/${file.relativeTo(entry).invariantSeparatorsPath}", layout)
                if (to.exists()) {
                    kept++
                } else {
                    to.parentFile?.mkdirs()
                    if (file.renameTo(to) || (runCatching { file.copyTo(to) }.isSuccess && file.delete())) moved++ else kept++
                }
            }
            if (entry.walkTopDown().none { it.isFile }) entry.deleteRecursively()
        }
        if (moved + kept > 0) {
            Log.i(TAG, "Switch data written in the app's own folder without folder access: $moved files moved to $switch, $kept kept in $fallback")
        }
    }

    /** Whether [dir] is [folder] or inside it. */
    internal fun isInside(dir: File, folder: File): Boolean =
        dir == folder || dir.path.startsWith(folder.path + File.separator)

    /**
     * Whether a folder Eden's settings name, outside the Punica folder and Eden's user folder, can be the owner's
     * choice: one that exists and can be read, not in another app's Android/data (Eden's own app, from an imported
     * config.ini), which Eden cannot use anyway. [ownFolder]: the app's own Android/data folder.
     */
    internal fun isOwnersChoice(dir: File, ownFolder: File?): Boolean {
        if (!dir.isDirectory || !dir.canRead()) return false
        val androidData = "${File.separator}Android${File.separator}data${File.separator}"
        if (!dir.path.contains(androidData)) return true
        val ownPackage = ownFolder?.path?.substringAfter(androidData, "")?.substringBefore(File.separator)
        return !ownPackage.isNullOrEmpty() && dir.path.substringAfter(androidData).substringBefore(File.separator) == ownPackage
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
