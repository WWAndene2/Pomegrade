package me.magnum.melonds.impl

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.DocumentsContract
import android.util.Log
import androidx.documentfile.provider.DocumentFile
import androidx.preference.PreferenceManager
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.runBlocking
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.domain.repositories.RomsRepository
import me.magnum.melonds.domain.repositories.SettingsRepository
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.activities.EmulationActivity
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.utils.CitraDirectoryHelper
import org.citra.citra_emu.utils.PermissionsHandler
import org.json.JSONArray
import java.io.File

/**
 * The Pomegrade folder (named Punica on the phone): one folder, picked once, where Punica keeps every console's
 * games and files, in the layout of [PunicaLayout] (one folder per console, the same six folders in each:
 * Games, Saves, States, System, Mods, Core). A folder in the earlier layout (Roms, BIOS, the cores' folders as
 * they were) is moved into it once ([applyLayout]).
 *
 * Games found in other ROM folders are moved into their console's Games. Moving uses file paths (java.io):
 * Android's document API can't move between two folders picked separately, and "All files access" is needed
 * anyway by the 3DS core, which reads its folder by path. Only the phone's storage provider (internal storage,
 * SD cards) is supported, as for the 3DS core.
 */
class PomegradeFolder(
    private val context: Context,
    private val settingsRepository: SettingsRepository,
    private val romsRepository: RomsRepository,
    private val screenshotProvider: SaveStateScreenshotProvider,
    private val settingsBackupManager: SettingsBackupManager,
) {

    companion object {
        // the earlier layout's top-level folders, moved by PunicaLayout.migrate; read until then
        const val ROMS = "Roms"
        const val BIOS = "BIOS"
        /** The owner's GBA BIOS, in GBA/System. */
        const val GBA_BIOS_FILE = "gba_bios.bin"
        internal val DS_BIOS_FILES = listOf("bios7.bin", "bios9.bin", "firmware.bin")
        internal val DSI_BIOS_FILES = DS_BIOS_FILES + "nand.bin"
        // what tells a DS bios7.bin from a DSi one, as the BIOS checks do
        internal const val DS_BIOS7_SIZE = 0x4000L
        internal const val DSI_BIOS7_SIZE = 0x10000L
        const val N3DS = PunicaLayout.N3DS
        const val SWITCH = PunicaLayout.SWITCH
        const val GC_WII = PunicaLayout.GC_WII
        const val BACKUPS = PunicaLayout.BACKUPS
        // a copy of the settings, the game list (each game's settings, last played, play time),
        // the controller and layout configurations, read back after a reinstall
        const val SETTINGS = PunicaLayout.SETTINGS

        /**
         * The Pomegrade folder's path once it is in the layout of [PunicaLayout], or null (not set up, no file access,
         * or still in the earlier layout: the cores then keep their earlier folders until the game list moves them).
         */
        fun layoutRoot(context: Context): File? = subFolder(context, "")?.takeIf { PunicaLayout.isMigrated(it) }

        /** One of a console's six folders ([PunicaLayout]), as a path in the Pomegrade folder, or null. */
        fun consoleFolder(context: Context, console: String, part: String): File? = subFolder(context, "$console/$part")
        /** Where Pomegrade creates its folder: /storage/emulated/0/Pomegrade. */
        // the app's visible name; folders made before 13 October were named Pomegrade (see PunicaFolderRename)
        const val DEFAULT_NAME = "Punica"
        const val PREFERENCE = "pomegrade_folder"
        private const val TAG = "PomegradeFolder"
        private const val STORAGE_PROVIDER = "com.android.externalstorage.documents"
        // DS save states: <ROM name>.ml0 (quick save) to .ml10
        private const val SAVE_STATE_SLOTS = FileSystemSaveStatesRepository.SAVE_STATE_SLOT_COUNT
        // GBA save states: <ROM name>.ss0 (quick save) to .ss9
        private const val GBA_STATE_SLOTS = 10

        /**
         * Moving games by path, and the 3DS core (Azahar's standard build, which reads its folder by
         * path), need "All files access" from Android 11, the storage permission on Android 10.
         */
        fun hasFileAccess(context: Context): Boolean {
            return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                Environment.isExternalStorageManager()
            } else {
                context.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
            }
        }

        /**
         * A sub-folder of the Pomegrade folder as a file path, for what is read and written by path
         * (texture packs, save state previews), or null if there is no Pomegrade folder or no file
         * access. Kept in the phone's shared storage, it stays when the app is uninstalled.
         */
        fun subFolder(context: Context, name: String): File? {
            val folder = PreferenceManager.getDefaultSharedPreferences(context).getString(PREFERENCE, null)?.let { Uri.parse(it) } ?: return null
            if (!hasFileAccess(context)) return null
            val id = try {
                DocumentsContract.getTreeDocumentId(folder)
            } catch (e: IllegalArgumentException) {
                return null
            }
            return pathOf("$id/$name")
        }

        /** The default folder (/storage/emulated/0/Punica), created if needed, as the folder picker's starting point. */
        fun createDefaultFolder(): Uri? {
            val dir = File(Environment.getExternalStorageDirectory(), DEFAULT_NAME)
            if (!dir.isDirectory && !dir.mkdirs()) return null
            return DocumentsContract.buildDocumentUri(STORAGE_PROVIDER, "primary:$DEFAULT_NAME")
        }

        /** The document ID of the Pomegrade folder's Roms ("primary:Pomegrade/Roms"), or null. */
        fun romsDocumentId(folder: Uri?): String? {
            folder ?: return null
            return try {
                DocumentsContract.getTreeDocumentId(folder) + "/" + ROMS
            } catch (e: IllegalArgumentException) {
                null
            }
        }

        /**
         * The document IDs of the Pomegrade folder's folders that hold no game: every console's Saves, States,
         * System, Mods and Core, Settings and Backups (and the earlier layout's BIOS, Textures and previews), so
         * the game list never looks in them. The Switch firmware's NCAs, in Switch/System, have a Switch game's
         * extension.
         */
        fun systemFolderDocumentIds(folder: Uri?): Set<String> {
            val root = treeIdOf(folder) ?: return emptySet()
            val parts = PunicaLayout.CONSOLES.flatMap { console -> (PunicaLayout.PARTS - PunicaLayout.GAMES).map { "$console/$it" } }
            return (parts + listOf(SETTINGS, BACKUPS, BIOS, "Textures", "Save state previews")).map { "$root/$it" }.toSet()
        }

        /** The document IDs of every console's Games ("primary:Punica/DS/Games"...), or none. */
        fun gamesDocumentIds(folder: Uri?): Set<String> {
            val root = treeIdOf(folder) ?: return emptySet()
            return PunicaLayout.CONSOLES.map { "$root/$it/${PunicaLayout.GAMES}" }.toSet()
        }

        private fun treeIdOf(folder: Uri?): String? {
            folder ?: return null
            return try {
                DocumentsContract.getTreeDocumentId(folder)
            } catch (e: IllegalArgumentException) {
                null
            }
        }

        /** The document ID of a tree or document URI of the storage provider, or null. */
        fun documentIdOf(context: Context, uri: Uri): String? {
            if (uri.authority != STORAGE_PROVIDER) return null
            return try {
                if (DocumentsContract.isDocumentUri(context, uri)) DocumentsContract.getDocumentId(uri) else DocumentsContract.getTreeDocumentId(uri)
            } catch (e: IllegalArgumentException) {
                null
            }
        }

        /** The file path of a storage provider document ID ("primary:Pomegrade" -> /storage/emulated/0/Pomegrade). */
        fun pathOf(documentId: String): File {
            val volume = documentId.substringBefore(':')
            val relative = documentId.substringAfter(':', "")
            val root = if (volume == "primary") Environment.getExternalStorageDirectory() else File("/storage/$volume")
            return if (relative.isEmpty()) root else File(root, relative)
        }
    }

    sealed class SetupResult {
        data object Success : SetupResult()
        /** Not on the phone's storage (cloud, another app): nothing can be moved there by path. */
        data object UnsupportedFolder : SetupResult()
        data object Failed : SetupResult()
    }

    data class OrganizeResult(val moved: Int, val failed: List<String>)

    fun isSetUp(): Boolean {
        val folder = settingsRepository.getPomegradeFolder() ?: return false
        return context.contentResolver.persistedUriPermissions.any { it.uri == folder && it.isWritePermission } &&
            DocumentFile.fromTreeUri(context, folder)?.exists() == true
    }

    /**
     * Makes [folder] (a tree picked with the system's folder picker) the Pomegrade folder: puts it in the layout of
     * [PunicaLayout] (moving an earlier layout found there), moves the games found so far into their console's Games
     * (with their saves, from wherever they are kept now), and points the DS, 3DS, Switch and GameCube / Wii
     * settings to it. Runs on a background thread.
     */
    fun setUp(folder: Uri, roms: List<Rom>, onProgress: (String) -> Unit): Pair<SetupResult, OrganizeResult?> {
        if (folder.authority != STORAGE_PROVIDER || documentIdOf(context, folder) == null) {
            return SetupResult.UnsupportedFolder to null
        }
        try {
            val flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            context.contentResolver.takePersistableUriPermission(folder, flags)
        } catch (e: SecurityException) {
            return SetupResult.Failed to null
        }
        val root = pathOf(DocumentsContract.getTreeDocumentId(folder))
        if (!root.isDirectory && !root.mkdirs()) return SetupResult.Failed to null
        settingsRepository.setPomegradeFolder(folder)
        applyLayout(roms)

        // games first, while the settings still say where their saves are now
        // the list as it is once applyLayout has moved the games of an earlier layout
        val organized = organize(runBlocking { romsRepository.getRoms().first() }, onProgress)

        // texture packs (and dumps) from the app's own folder, which Android deletes with the app
        context.getExternalFilesDir("textures")?.listFiles()?.forEach { game ->
            val target = File(PunicaLayout.part(root, PunicaLayout.DS, PunicaLayout.MODS), game.name)
            if (!target.exists()) game.renameTo(target) || game.copyRecursively(target) && game.deleteRecursively()
        }
        screenshotProvider.moveTo(File(PunicaLayout.part(root, PunicaLayout.DS, PunicaLayout.STATES), PunicaLayout.PREVIEWS))

        organizeSaves()
        organizeBios()
        setUpThreeDs()
        hideImagesFromGallery()
        return SetupResult.Success to organized
    }

    /**
     * Puts the Pomegrade folder in the layout of [PunicaLayout], once (owner's request, 15 October): moves the earlier
     * one ([PunicaLayout.migrate]), then follows the moved games in the game list (their settings, play time and state
     * screenshots go with them), scans every console's Games instead of Roms, and moves the debug trace folders kept
     * in the settings. Background thread, before the game list scans.
     */
    fun applyLayout(roms: List<Rom>) {
        val folder = settingsRepository.getPomegradeFolder() ?: return
        if (!hasFileAccess(context)) return
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        val root = pathOf(treeId)
        if (!PunicaLayout.isMigrated(root)) {
            val notes = mapOf(
                PunicaLayout.N3DS to (context.getString(R.string.punica_layout_saves_note_file) to context.getString(R.string.punica_layout_saves_note_three_ds)),
                PunicaLayout.GC_WII to (context.getString(R.string.punica_layout_saves_note_file) to context.getString(R.string.punica_layout_saves_note_gc_wii)),
            )
            val report = PunicaLayout.migrate(root, notes)
            report.left.forEach { Log.w(TAG, "Punica layout: left $it") }
            val rootPath = root.canonicalPath
            fun idOf(file: File) = treeId + file.canonicalPath.removePrefix(rootPath)
            roms.forEach { rom ->
                val old = documentIdOf(context, rom.uri)?.let { pathOf(it) } ?: return@forEach
                val new = report.games[old] ?: return@forEach
                val newUri = DocumentsContract.buildDocumentUriUsingTree(folder, idOf(new))
                val parentUri = DocumentsContract.buildDocumentUriUsingTree(folder, idOf(new.parentFile!!))
                screenshotProvider.relocateRomScreenshots(rom, newUri)
                romsRepository.relocateRom(rom, newUri, parentUri)
            }
            moveTraceFolder("ds_debug_trace_folder", "${PunicaLayout.DS}/Traces", "${PunicaLayout.DS}/${PunicaLayout.CORE}/Traces", root)
            moveTraceFolder(EmulationActivity.CODE_TRACE_FOLDER, "${PunicaLayout.N3DS}/Traces", "${PunicaLayout.N3DS}/${PunicaLayout.CORE}/Traces", root)
        }
        // every console's Games scanned, the earlier Roms no more
        settingsRepository.excludeRomSearchDirectory(DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$ROMS"))
        gamesDocumentIds(folder).forEach { settingsRepository.includeRomSearchDirectory(DocumentsContract.buildDocumentUriUsingTree(folder, it)) }
    }

    // a trace folder kept as a path in the settings, moved with the layout
    private fun moveTraceFolder(key: String, before: String, after: String, root: File) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val path = preferences.getString(key, null) ?: return
        if (File(path).canonicalPath == File(root, before).canonicalPath) preferences.edit().putString(key, File(root, after).absolutePath).apply()
    }

    /**
     * Points the DS and DSi BIOS settings to DS/System and DS/System/DSi, unless they point elsewhere (the owner's
     * own folder), and copies the system files a core reads from its own folder ([ConsoleSystemFiles]). Runs on a
     * background thread, at setup and when the game list opens.
     */
    fun organizeBios() {
        val folder = settingsRepository.getPomegradeFolder() ?: return
        if (!hasFileAccess(context)) return
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        val root = pathOf(treeId)
        // the settings follow the files: not before applyLayout has moved them
        if (!PunicaLayout.isMigrated(root)) return
        val system = "${PunicaLayout.DS}/${PunicaLayout.SYSTEM}"
        File(root, "$system/${PunicaLayout.DSI}").mkdirs()
        fun uri(path: String) = DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$path")
        // the Punica folder's own BIOS folders, before and after the layout: replaced; any other is the owner's choice
        val earlier = setOf(uri(BIOS), uri("$BIOS/DS"), uri("$BIOS/DSi"))
        val dsBios = settingsRepository.getDsBiosDirectory()
        if (dsBios == null || dsBios in earlier) settingsRepository.setDsBiosDirectory(uri(system))
        val dsiBios = settingsRepository.getDsiBiosDirectory()
        if (dsiBios == null || dsiBios in earlier) settingsRepository.setDsiBiosDirectory(uri("$system/${PunicaLayout.DSI}"))
        // the GameCube boot ROM's region folders, so where to put IPL.bin is plain
        ConsoleSystemFiles.GC_REGIONS.forEach { File(PunicaLayout.part(root, PunicaLayout.GC_WII, PunicaLayout.SYSTEM), it).mkdirs() }
        ConsoleSystemFiles.import(root)
    }

    /** The system files checked and put in place ([SystemFilesCheck]): moved, never deleted. Background thread. */
    fun checkSystemFiles(): SystemFilesCheck.Report = SystemFilesCheck.run(context, settingsRepository)

    /**
     * Points the DS saves setting to DS/Saves and the DS states to DS/States, unless the user set another folder
     * (the earlier layout's DS folder is replaced). Runs on a background thread, at setup and when the game list
     * opens.
     */
    fun organizeSaves() {
        val folder = settingsRepository.getPomegradeFolder() ?: return
        if (!hasFileAccess(context)) return
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        // the settings follow the files: not before applyLayout has moved them
        if (!PunicaLayout.isMigrated(pathOf(treeId))) return
        fun uri(path: String) = DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$path")
        val saves = uri("${PunicaLayout.DS}/${PunicaLayout.SAVES}")
        val current = runCatching { settingsRepository.getSaveFileDirectory() }.getOrNull()
        if (settingsRepository.saveNextToRomFile() || current == null || current == uri(PunicaLayout.DS) || current == saves) {
            settingsRepository.keepGameFilesInConsoleFolder(saves, uri("${PunicaLayout.DS}/${PunicaLayout.STATES}"))
        }
    }

    /** The folder path, or null if not set up. */
    fun path(): File? {
        val folder = settingsRepository.getPomegradeFolder() ?: return null
        return try {
            pathOf(DocumentsContract.getTreeDocumentId(folder))
        } catch (e: IllegalArgumentException) {
            null
        }
    }

    private fun settingsFolder(): Uri? {
        val folder = settingsRepository.getPomegradeFolder() ?: return null
        if (!isSetUp()) return null
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        val dir = pathOf("$treeId/$SETTINGS")
        if (!dir.isDirectory && !dir.mkdirs()) return null
        return DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$SETTINGS")
    }

    /** Saves the settings in the folder's Settings (each save replaces the previous; one at a time). Background thread. */
    @Synchronized
    fun saveSettings(): Boolean {
        val target = settingsFolder() ?: return false
        return runCatching { settingsBackupManager.backup(target) }.isSuccess
    }

    /** Settings saved before in the folder (a previous installation's), or none. */
    fun hasSavedSettings(): Boolean = path()?.let { File(it, "$SETTINGS/settings.json").isFile } == true

    /** Restores the settings saved in the folder, and the game list with them. Background thread. */
    @Synchronized
    fun restoreSettings(): Boolean {
        val source = settingsFolder() ?: return false
        val restored = runCatching { settingsBackupManager.restore(source) }.isSuccess
        if (restored) {
            runCatching { relocateRestoredGames() }
            romsRepository.reloadRoms()
        }
        return restored
    }

    /**
     * The game list just restored (saved before the folder had the layout of [PunicaLayout]) names games in the
     * earlier Roms: each one the layout moved is pointed to its new place ([PunicaLayout.movedGame]), with its state
     * screenshots, before the list is read (a game whose file is missing is dropped from it); so are the debug trace
     * folders the restored settings name.
     */
    private fun relocateRestoredGames() {
        val folder = settingsRepository.getPomegradeFolder() ?: return
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        val root = pathOf(treeId)
        if (!PunicaLayout.isMigrated(root) || !hasFileAccess(context)) return
        val file = File(context.filesDir, "rom_data.json").takeIf { it.isFile } ?: return
        val games = JSONArray(file.readText())
        val rootPath = root.canonicalPath
        fun uriOf(path: File) = DocumentsContract.buildDocumentUriUsingTree(folder, treeId + path.canonicalPath.removePrefix(rootPath))
        var changed = false
        for (i in 0 until games.length()) {
            val game = games.getJSONObject(i)
            val oldUri = Uri.parse(game.optString("uri"))
            val old = documentIdOf(context, oldUri)?.let { pathOf(it) } ?: continue
            if (old.exists()) continue
            val moved = PunicaLayout.movedGame(root, old) ?: continue
            val newUri = uriOf(moved)
            game.put("uri", newUri.toString())
            game.put("parentTreeUri", uriOf(moved.parentFile!!).toString())
            screenshotProvider.relocateRomScreenshots(oldUri, newUri)
            changed = true
        }
        if (changed) file.writeText(games.toString())
        // the restored settings' trace folders, as applyLayout moved them
        moveTraceFolder("ds_debug_trace_folder", "${PunicaLayout.DS}/Traces", "${PunicaLayout.DS}/${PunicaLayout.CORE}/Traces", root)
        moveTraceFolder(EmulationActivity.CODE_TRACE_FOLDER, "${PunicaLayout.N3DS}/Traces", "${PunicaLayout.N3DS}/${PunicaLayout.CORE}/Traces", root)
    }

    /** Set up, with the file access moving games needs. */
    fun canOrganize(): Boolean = isSetUp() && hasFileAccess(context)

    /**
     * The 3DS core's folder: the Pomegrade folder's 3DS, unless it was already set up elsewhere (its
     * data stays there). Also called when the Pomegrade folder is set up but this part isn't (an
     * earlier setup stopped before it). Returns whether the 3DS core has its folder. Not on 32-bit
     * devices: there is no 3DS core there.
     */
    fun setUpThreeDs(): Boolean {
        if (!CitraApplication.isSupported) return false
        val folder = settingsRepository.getPomegradeFolder() ?: return false
        // Azahar starts its runtime when one of its screens first opens; its folder setup needs it
        // (DocumentsTree): started here first
        CitraApplication.start()
        if (PermissionsHandler.hasWriteAccess(context)) return true
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        val dir = pathOf("$treeId/$N3DS")
        if (!dir.isDirectory && !dir.mkdirs()) return false
        CitraDirectoryHelper.initializeCitraDirectory(DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$N3DS"))
        // Azahar's own first-time setup would ask for a folder again if its home screen opens
        PreferenceManager.getDefaultSharedPreferences(context).edit()
            .putBoolean(Settings.PREF_FIRST_APP_LAUNCH, false)
            .apply()
        return PermissionsHandler.hasWriteAccess(context)
    }

    /**
     * Keeps the image files the emulators write out of the phone's gallery: a `.nomedia` file in DS/Mods (the DS
     * texture packs, <game code>/, their dumps in dump/), DS/States/Previews (the state screenshots) and the 3DS
     * core's dumps (3DS/Core/dump/textures/<title id>/), which Android's media scanner otherwise lists as photos
     * (the owner, 8 October: the dumped textures filled the gallery). A `.nomedia` hides the folder's sub-folders
     * too; files already listed leave the gallery when it next rescans.
     */
    fun hideImagesFromGallery() {
        val root = path() ?: return
        if (!hasFileAccess(context)) return
        val folders = listOf(
            PunicaLayout.part(root, PunicaLayout.DS, PunicaLayout.MODS),
            File(PunicaLayout.part(root, PunicaLayout.DS, PunicaLayout.STATES), PunicaLayout.PREVIEWS),
            File(PunicaLayout.part(root, PunicaLayout.N3DS, PunicaLayout.CORE), "dump"),
        )
        for (dir in folders) {
            runCatching {
                if (dir.isDirectory || dir.mkdirs()) File(dir, ".nomedia").createNewFile()
            }
        }
    }

    /** The games of [roms] not in their console's Games yet. */
    fun gamesToOrganize(roms: List<Rom>): List<Rom> {
        val games = gamesDocumentIds(settingsRepository.getPomegradeFolder()).takeIf { it.isNotEmpty() } ?: return emptyList()
        return roms.filter { rom ->
            val id = documentIdOf(context, rom.uri)
            id != null && games.none { id.startsWith("$it/") }
        }
    }

    /**
     * Moves each game of [roms] not in its console's Games yet into it, its DS or GBA saves and states into that
     * console's Saves and States, and updates its entry (settings, last played and play time follow it). A game that
     * can't be moved (a file of the same name already there) stays where it is. Runs on a background thread.
     */
    fun organize(roms: List<Rom>, onProgress: (String) -> Unit): OrganizeResult {
        val folder = settingsRepository.getPomegradeFolder() ?: return OrganizeResult(0, emptyList())
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        var moved = 0
        val failed = ArrayList<String>()
        for (rom in gamesToOrganize(roms)) {
            onProgress(rom.name.ifEmpty { rom.fileName })
            if (moveGame(rom, folder, treeId)) moved++ else failed.add(rom.fileName)
        }
        return OrganizeResult(moved, failed)
    }

    private fun moveGame(rom: Rom, folder: Uri, treeId: String): Boolean {
        val romId = documentIdOf(context, rom.uri) ?: return false
        val source = pathOf(romId)
        if (!source.isFile) return false
        val console = PunicaLayout.console(rom.platform)
        val root = pathOf(treeId)

        // the files that go with it, found where the settings keep them now
        val baseName = source.nameWithoutExtension
        val saves = ArrayList<File>()
        val states = ArrayList<File>()
        if (rom.platform == RomPlatform.NDS) {
            fileDirectory { settingsRepository.getSaveFileDirectory(rom) }?.let { saves.add(File(it, "$baseName.sav")) }
            fileDirectory { settingsRepository.getSaveStateDirectory(rom) }?.let { dir ->
                for (slot in 0 until SAVE_STATE_SLOTS) states.add(File(dir, "$baseName.ml$slot"))
            }
        } else if (rom.platform == RomPlatform.GBA) {
            // kept next to the game before (GbaEmulatorActivity): its save, its states and their previews
            val dir = source.parentFile
            saves.add(File(dir, "$baseName.sav"))
            for (slot in 0 until GBA_STATE_SLOTS) {
                states.add(File(dir, "$baseName.ss$slot"))
                states.add(File(dir, "$baseName.ss$slot.png"))
            }
        }

        val target = File(PunicaLayout.part(root, console, PunicaLayout.GAMES), source.name)
        target.parentFile?.mkdirs()
        if (target.exists() || !GameFolderMover.moveFile(source, target)) return false
        // its saves and states into its console's folders (a file already there is left where it is)
        listOf(saves to PunicaLayout.SAVES, states to PunicaLayout.STATES).forEach { (files, part) ->
            val dir = PunicaLayout.part(root, console, part)
            if (files.isNotEmpty() && (dir.isDirectory || dir.mkdirs())) {
                files.filter { it.isFile && it.parentFile?.canonicalFile != dir.canonicalFile }
                    .forEach { GameFolderMover.moveFile(it, File(dir, it.name)) }
            }
        }

        val gamesId = "$treeId/$console/${PunicaLayout.GAMES}"
        val newUri = DocumentsContract.buildDocumentUriUsingTree(folder, "$gamesId/${source.name}")
        val parentUri = DocumentsContract.buildDocumentUriUsingTree(folder, gamesId)
        screenshotProvider.relocateRomScreenshots(rom, newUri)
        romsRepository.relocateRom(rom, newUri, parentUri)
        return true
    }

    // the file path of a folder the settings give as a URI (document or file), or null
    private fun fileDirectory(getUri: () -> Uri?): File? {
        val uri = try {
            getUri()
        } catch (e: Exception) {
            null
        } ?: return null
        if (uri.scheme == "file") return uri.path?.let { File(it) }
        return documentIdOf(context, uri)?.let { pathOf(it) }
    }
}
