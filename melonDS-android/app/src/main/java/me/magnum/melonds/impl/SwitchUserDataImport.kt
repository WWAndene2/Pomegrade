package me.magnum.melonds.impl

import android.content.Context
import android.net.Uri
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

/**
 * "Import Switch data" (Settings > Switch): brings Switch data into the Punica folder's Switch, each part of Eden's
 * user folder where [SwitchFolder.placeOf] puts it (saves in Saves, NAND and SD card in System, mods in Mods, the
 * rest in Core), from two places:
 * - the app's own folder (Android/data), where Eden's data stays when Pomegrade/Switch already had the same
 *   entry ([SwitchFolder.moveUserData] never overwrites): its files are added, the files already in
 *   Pomegrade/Switch are kept;
 * - a backup exported by Eden ("Export user data": a zip holding config/config.ini), from Eden's own app for
 *   example: its files replace the same files in Pomegrade/Switch, after the folder is saved in a zip in
 *   Pomegrade/Backups.
 * Eden's own import deletes its user folder before unzipping: not used, that folder holds the Switch saves.
 * Background thread.
 */
object SwitchUserDataImport {

    sealed class Result {
        /** [files] files written; the folder as it was before, when files were replaced, in [backup]. */
        data class Done(val files: Int, val backup: File?) : Result()
        data object NoFolder : Result()
        data object NothingToImport : Result()
        data object NotEdenBackup : Result()
        data class Failed(val error: Throwable) : Result()
    }

    // nand and sdmc included: they stay in the app's folder when Pomegrade/Switch already had them
    private val ENTRIES = listOf("nand", "sdmc", "config", "keys", "log", "shader", "load", "dump", "screenshots", "amiibo", "play_time", "icons", "crash_dumps", "tas", "cache")
    private const val EDEN_CONFIG = "config/config.ini"

    /** Eden's entries still in the app's own folder, besides Pomegrade/Switch. */
    fun oldDataEntries(context: Context): List<File> {
        val target = target(context) ?: return emptyList()
        val source = context.getExternalFilesDir(null) ?: return emptyList()
        if (source.canonicalFile == target.canonicalFile) return emptyList()
        return ENTRIES.map { File(source, it) }.filter { it.exists() }
    }

    /** Adds the files of the app's own folder that Pomegrade/Switch doesn't have; the old copies are then deleted. */
    fun importOldData(context: Context): Result {
        val target = target(context) ?: return Result.NoFolder
        val entries = oldDataEntries(context)
        if (entries.isEmpty()) return Result.NothingToImport
        return withEdenConfigUnloaded {
            var written = 0
            entries.forEach { entry ->
                entry.walkTopDown().filter { it.isFile }.forEach { file ->
                    val to = SwitchFolder.placeOf(target, "${entry.name}/${file.relativeTo(entry).invariantSeparatorsPath}", layout(context))
                    if (!to.exists()) {
                        file.copyTo(to)
                        written++
                    }
                }
                // every file is now in Pomegrade/Switch, the same one or the copy kept there
                entry.deleteRecursively()
            }
            Result.Done(written, null)
        }
    }

    /** Replaces Pomegrade/Switch's files with those of a zip exported by Eden, after a backup of the folder. */
    fun importBackup(context: Context, zip: Uri): Result {
        val target = target(context) ?: return Result.NoFolder
        val isEdenBackup = runCatching {
            ZipInputStream(context.contentResolver.openInputStream(zip)!!.buffered()).use { stream ->
                generateSequence { stream.nextEntry }.any { it.name.trim().trimStart('/') == EDEN_CONFIG }
            }
        }.getOrDefault(false)
        if (!isEdenBackup) return Result.NotEdenBackup
        return withEdenConfigUnloaded {
            val backup = backup(context, target)
            var written = 0
            val root = target.canonicalFile
            val layout = layout(context)
            ZipInputStream(context.contentResolver.openInputStream(zip)!!.buffered()).use { stream ->
                generateSequence { stream.nextEntry }.forEach { entry ->
                    val to = SwitchFolder.placeOf(root, entry.name.trimStart('/').trimEnd('/'), layout).canonicalFile
                    // a zip entry may not write outside the folder ("../")
                    if (!to.path.startsWith(root.path + File.separator)) return@forEach
                    if (entry.isDirectory) {
                        to.mkdirs()
                    } else {
                        to.parentFile?.mkdirs()
                        to.outputStream().use { stream.copyTo(it) }
                        written++
                    }
                }
            }
            Result.Done(written, backup)
        }
    }

    private fun target(context: Context): File? =
        PomegradeFolder.subFolder(context, PomegradeFolder.SWITCH)?.takeIf { it.isDirectory || it.mkdirs() }

    private fun layout(context: Context) = PomegradeFolder.layoutRoot(context) != null

    /** Pomegrade/Switch as it is, in Pomegrade/Backups/Switch-<date>.zip, or null when it holds no file. */
    private fun backup(context: Context, folder: File): File? {
        // the games are not Switch data, and too large for a backup
        val games = File(folder, PunicaLayout.GAMES)
        val files = folder.walkTopDown().onEnter { it != games }.filter { it.isFile }.toList()
        if (files.isEmpty()) return null
        val backups = PomegradeFolder.subFolder(context, PomegradeFolder.BACKUPS)?.takeIf { it.isDirectory || it.mkdirs() }
            ?: throw IllegalStateException("no Backups folder")
        val name = "Switch-" + SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date()) + ".zip"
        val zip = File(backups, name)
        ZipOutputStream(zip.outputStream().buffered()).use { out ->
            files.forEach { file ->
                out.putNextEntry(ZipEntry(file.relativeTo(folder).invariantSeparatorsPath))
                file.inputStream().use { it.copyTo(out) }
                out.closeEntry()
            }
        }
        return zip
    }

    /**
     * Runs [block] with Eden's settings unloaded when Eden has started, then reloads them, as Eden's own
     * import does: otherwise Eden would write its settings in memory over the imported ones.
     */
    private fun withEdenConfigUnloaded(block: () -> Result): Result {
        val started = YuzuApplication.isStarted
        if (started) NativeConfig.unloadGlobalConfig()
        return try {
            block()
        } catch (e: Exception) {
            Result.Failed(e)
        } finally {
            if (started) {
                NativeLibrary.initializeSystem(true)
                NativeConfig.initializeGlobalConfig()
            }
        }
    }
}
