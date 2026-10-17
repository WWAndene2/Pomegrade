package me.magnum.melonds.impl

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import androidx.preference.PreferenceManager
import org.json.JSONArray
import java.io.File

/**
 * Renames the user's folder from Pomegrade to Punica, the app's visible name (owner's decision, 13 October),
 * keeping everything in it. Android gives a folder's access per folder, through its picker only: once the
 * folder is renamed ([rename]), the user confirms the new one in the picker, and [complete] then rewrites
 * every place the old location was kept in:
 * - the app's preferences (the folder, the ROM folders, the DS and DSi BIOS folders, the 3DS core's folder,
 *   the trace folders; the cores' preferences are in the same file);
 * - the game list (rom_data.json: each game's URI);
 * - the cores' settings in the folder (Switch/config, GC-Wii/Config, 3DS/config, and their places in the
 *   layout of PunicaLayout: <console>/Core/...) and the saved settings (Settings), which name paths or URIs in it;
 * - the DS state screenshots, kept under each game's URI (SaveStateScreenshotProvider).
 * Never while the folder's files are moved into the layout (PunicaLayout.FOLDER_LOCK); the cores recompute
 * their paths afterwards.
 * Both forms are rewritten: the path (/storage/emulated/0/Pomegrade) and the storage provider's document
 * ID as it appears in URIs (primary%3APomegrade). The app restarts afterwards, so nothing keeps an old one.
 * Until [complete], the step done is kept ([isPending]): an interrupted rename resumes, or is undone
 * ([revert]). Only a folder named Pomegrade is renamed; one the user named otherwise keeps its name.
 */
object PunicaFolderRename {

    private const val OLD_NAME = "Pomegrade"
    private const val NEW_NAME = PomegradeFolder.DEFAULT_NAME
    // the old folder's document ID while the folder is renamed but the new one not confirmed yet
    private const val PENDING = "punica_folder_rename_from"
    private const val STORAGE_PROVIDER = "com.android.externalstorage.documents"
    // characters that may continue a name: what follows a replaced location must not be one
    private const val NAME_CHAR = "[A-Za-z0-9 ._~()-]"

    sealed class RenameResult {
        data object Renamed : RenameResult()
        data object TargetExists : RenameResult()
        data object Failed : RenameResult()
    }

    /** The folder is named Pomegrade, reachable by path, and not being renamed. */
    fun isNeeded(context: Context): Boolean {
        if (isPending(context) || !PomegradeFolder.hasFileAccess(context)) return false
        val id = currentId(context) ?: return false
        return id.substringAfterLast('/').substringAfterLast(':') == OLD_NAME && PomegradeFolder.pathOf(id).isDirectory
    }

    fun isPending(context: Context): Boolean = preferences(context).getString(PENDING, null) != null

    /** The picker's starting point and the folder to confirm: the renamed folder. */
    fun newFolderUri(context: Context): Uri? = pendingIds(context)?.let { (_, new) -> DocumentsContract.buildDocumentUri(STORAGE_PROVIDER, new) }

    /** Renames the folder on the phone; the new one is then confirmed with [complete]. Background thread. */
    fun rename(context: Context): RenameResult = synchronized(PunicaLayout.FOLDER_LOCK) {
        val oldId = currentId(context) ?: return RenameResult.Failed
        val newId = oldId.dropLast(OLD_NAME.length) + NEW_NAME
        val from = PomegradeFolder.pathOf(oldId)
        val to = PomegradeFolder.pathOf(newId)
        if (to.exists()) {
            // an empty Punica (made by a newer installation's picker) is no reason to stop
            if (!(to.isDirectory && to.list().isNullOrEmpty() && to.delete())) return RenameResult.TargetExists
        }
        // remembered first: if the app stops right after, the next start knows the folder moved
        preferences(context).edit().putString(PENDING, oldId).commit()
        if (!from.renameTo(to)) {
            preferences(context).edit().remove(PENDING).commit()
            return RenameResult.Failed
        }
        return RenameResult.Renamed
    }

    /** Puts the folder's old name back (the user didn't confirm the new folder). Background thread. */
    fun revert(context: Context): Boolean = synchronized(PunicaLayout.FOLDER_LOCK) {
        val (oldId, newId) = pendingIds(context) ?: return false
        val from = PomegradeFolder.pathOf(newId)
        val to = PomegradeFolder.pathOf(oldId)
        if (to.exists() || !from.renameTo(to)) return false
        preferences(context).edit().remove(PENDING).commit()
        return true
    }

    /**
     * Makes [picked] (the tree the user confirmed in the picker) the folder and rewrites the old location
     * everywhere. Returns false when [picked] isn't the renamed folder. Background thread.
     */
    fun complete(context: Context, picked: Uri): Boolean = synchronized(PunicaLayout.FOLDER_LOCK) {
        val (oldId, newId) = pendingIds(context) ?: return false
        val pickedId = runCatching { DocumentsContract.getTreeDocumentId(picked) }.getOrNull()
        if (picked.authority != STORAGE_PROVIDER || pickedId != newId) return false
        val flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        context.contentResolver.takePersistableUriPermission(picked, flags)

        val rewrite = rewriter(oldId, newId)
        val preferences = preferences(context)
        val oldFolder = preferences.getString(PomegradeFolder.PREFERENCE, null)?.let { Uri.parse(it) }
        val editor = preferences.edit()
        preferences.all.forEach { (key, value) ->
            when (value) {
                is String -> rewrite(value).takeIf { it != value }?.let { editor.putString(key, it) }
                is Set<*> -> {
                    val strings = value.filterIsInstance<String>()
                    val rewritten = strings.map(rewrite)
                    if (rewritten != strings) editor.putStringSet(key, rewritten.toSet())
                }
            }
        }
        editor.putString(PomegradeFolder.PREFERENCE, picked.toString())
        editor.remove(PENDING)

        val folder = PomegradeFolder.pathOf(newId)
        val romData = File(context.filesDir, "rom_data.json")
        // each game's URI before and after, for its state screenshots
        val renamedRoms = runCatching {
            val games = JSONArray(romData.readText())
            (0 until games.length()).mapNotNull { i ->
                val uri = games.getJSONObject(i).optString("uri").takeIf { it.isNotEmpty() } ?: return@mapNotNull null
                rewrite(uri).takeIf { it != uri }?.let { Uri.parse(uri) to Uri.parse(it) }
            }
        }.getOrDefault(emptyList())
        val files = listOf(romData) +
            listOf(
                "Switch/config", "GC-Wii/Config", "3DS/config",
                "Switch/Core/config", "GC-Wii/Core/Config", "3DS/Core/config", PomegradeFolder.SETTINGS,
            )
                .flatMap { dir -> File(folder, dir).walkTopDown().filter { it.isFile && (it.extension == "ini" || it.extension == "json") }.toList() }
        files.filter { it.isFile }.forEach { file ->
            val text = file.readText()
            val rewritten = rewrite(text)
            if (rewritten != text) file.writeText(rewritten)
        }
        editor.commit()
        // after the commit: the screenshots' folder is then found in the renamed folder
        renamedRoms.forEach { (old, new) -> SaveStateScreenshotProvider.relocateRomScreenshots(context, old, new) }
        PomegradeFolder.onUserFolderChanged(context)

        // the old folder's access, no longer of any use
        oldFolder?.let { runCatching { context.contentResolver.releasePersistableUriPermission(it, flags) } }
        return true
    }

    /**
     * [uri] with the old folder's location replaced by the renamed folder's, for a URI kept outside the app
     * (a home-screen shortcut's); null when the folder isn't one renamed from Pomegrade.
     */
    fun renamedUri(context: Context, uri: Uri): Uri? {
        if (isPending(context)) return null
        val newId = currentId(context)?.takeIf { it.endsWith(NEW_NAME) } ?: return null
        val oldId = newId.dropLast(NEW_NAME.length) + OLD_NAME
        return Uri.parse(rewriter(oldId, newId)(uri.toString())).takeIf { it != uri }
    }

    private fun rewriter(oldId: String, newId: String): (String) -> String = rewriter(
        listOf(
            Uri.encode(oldId) to Uri.encode(newId),
            PomegradeFolder.pathOf(oldId).absolutePath to PomegradeFolder.pathOf(newId).absolutePath,
        )
    )

    /** Replaces each old location with its new one where the old one isn't the start of a longer name. */
    internal fun rewriter(locations: List<Pair<String, String>>): (String) -> String {
        val pairs = locations.map { (old, new) -> Regex(Regex.escape(old) + "(?!$NAME_CHAR)") to new }
        return { text -> pairs.fold(text) { result, (regex, new) -> regex.replace(result, Regex.escapeReplacement(new)) } }
    }

    private fun currentId(context: Context): String? {
        val folder = preferences(context).getString(PomegradeFolder.PREFERENCE, null)?.let { Uri.parse(it) } ?: return null
        return runCatching { DocumentsContract.getTreeDocumentId(folder) }.getOrNull()
    }

    private fun pendingIds(context: Context): Pair<String, String>? {
        val oldId = preferences(context).getString(PENDING, null) ?: return null
        return oldId to oldId.dropLast(OLD_NAME.length) + NEW_NAME
    }

    private fun preferences(context: Context) = PreferenceManager.getDefaultSharedPreferences(context)
}
