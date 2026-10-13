package me.magnum.melonds.impl

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.DocumentsContract
import androidx.documentfile.provider.DocumentFile
import androidx.preference.PreferenceManager
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.domain.repositories.RomsRepository
import me.magnum.melonds.domain.repositories.SettingsRepository
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.utils.CitraDirectoryHelper
import org.citra.citra_emu.utils.PermissionsHandler
import java.io.File

/**
 * The Pomegrade folder: one folder, picked once, where Pomegrade keeps the games and their files
 * for every console:
 *
 * ```
 * Punica/
 * ├── Roms/<game>/   each game's ROM
 * ├── DS/            the DS and DSi saves and save states (ConsoleSavesMover moved those kept next to the games)
 * ├── GBA/           the GBA saves, save states and their previews
 * ├── BIOS/          one folder per console:
 * │   ├── DS/        bios7.bin, bios9.bin, firmware.bin (when no other DS BIOS folder was set)
 * │   ├── DSi/       bios7.bin, bios9.bin, firmware.bin, nand.bin (likewise)
 * │   ├── GBA/       gba_bios.bin (optional, mGBA's own BIOS otherwise)
 * │   ├── Switch/    prod.keys, title.keys, Firmware/ (see SwitchSystemFiles)
 * │   ├── 3DS/       aes_keys.txt, boot9.bin... (optional; see ConsoleSystemFiles)
 * │   └── GC-Wii/    USA/, EUR/, JAP/ with the GameCube's IPL.bin (optional; see ConsoleSystemFiles)
 * ├── 3DS/           the 3DS core's folder (Azahar: 3DS saves, save states, system files)
 * ├── Switch/        the Switch core's whole folder (Eden: saves, updates, DLC, settings, keys, shaders; see SwitchFolder)
 * ├── GC-Wii/        the GameCube and Wii core's folder (Dolphin: memory cards, Wii saves, settings; see GcWiiFolder)
 * └── Backups/       a console's folder saved before data is imported over it (see SwitchUserDataImport)
 * ```
 *
 * Games found in other ROM folders are moved into Roms, each into a folder named after its file.
 * Moving uses file paths (java.io): Android's document API can't move between two folders picked
 * separately, and "All files access" is needed anyway by the 3DS core, which reads its folder by
 * path. Only the phone's storage provider (internal storage, SD cards) is supported, as for the
 * 3DS core.
 */
class PomegradeFolder(
    private val context: Context,
    private val settingsRepository: SettingsRepository,
    private val romsRepository: RomsRepository,
    private val screenshotProvider: SaveStateScreenshotProvider,
    private val settingsBackupManager: SettingsBackupManager,
) {

    companion object {
        const val ROMS = "Roms"
        const val BIOS = "BIOS"
        // BIOS has one folder per console: the DS and DSi files have the same names (bios7.bin, bios9.bin,
        // firmware.bin), so they can't share one
        const val BIOS_DS = "DS"
        const val BIOS_DSI = "DSi"
        const val BIOS_GBA = "GBA"
        const val BIOS_SWITCH = "Switch"
        /** The owner's GBA BIOS, in BIOS/GBA. */
        const val GBA_BIOS_FILE = "gba_bios.bin"
        private val BIOS_CONSOLES = listOf(BIOS_DS, BIOS_DSI, BIOS_GBA, BIOS_SWITCH, N3DS, GC_WII)
        private val DS_BIOS_FILES = listOf("bios7.bin", "bios9.bin", "firmware.bin")
        private val DSI_BIOS_FILES = DS_BIOS_FILES + "nand.bin"
        // what tells a DS bios7.bin from a DSi one, as the BIOS checks do
        private const val DS_BIOS7_SIZE = 0x4000L
        private const val DSI_BIOS7_SIZE = 0x10000L
        const val N3DS = "3DS"
        const val SWITCH = "Switch"
        const val GC_WII = "GC-Wii"
        const val BACKUPS = "Backups"
        // the DS (and DSi) and GBA saves and save states, as each console's are in its own folder
        const val DS_SAVES = "DS"
        const val GBA_SAVES = "GBA"
        const val TEXTURES = "Textures"
        const val SAVE_STATE_PREVIEWS = "Save state previews"
        // a copy of the settings, the game list (each game's settings, last played, play time),
        // the controller and layout configurations, read back after a reinstall
        const val SETTINGS = "Settings"
        /** Where Pomegrade creates its folder: /storage/emulated/0/Pomegrade. */
        // the app's visible name; folders made before 13 October were named Pomegrade (see PunicaFolderRename)
        const val DEFAULT_NAME = "Punica"
        const val PREFERENCE = "pomegrade_folder"
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
     * Makes [folder] (a tree picked with the system's folder picker) the Pomegrade folder: creates
     * its sub-folders, points the DS and 3DS settings to them, and moves the games found so far
     * into Roms (with their saves, from wherever they are kept now). Runs on a background thread.
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
        for (name in listOf(ROMS, BIOS, N3DS, TEXTURES, DS_SAVES, GBA_SAVES)) {
            val dir = File(root, name)
            if (!dir.isDirectory && !dir.mkdirs()) {
                return SetupResult.Failed to null
            }
        }
        settingsRepository.setPomegradeFolder(folder)

        // games first, while the settings still say where their saves are now
        val organized = organize(roms, onProgress)

        // texture packs (and dumps) from the app's own folder, which Android deletes with the app
        context.getExternalFilesDir("textures")?.listFiles()?.forEach { game ->
            val target = File(File(root, TEXTURES), game.name)
            if (!target.exists()) game.renameTo(target) || game.copyRecursively(target) && game.deleteRecursively()
        }
        screenshotProvider.moveTo(File(root, SAVE_STATE_PREVIEWS))

        val treeId = DocumentsContract.getTreeDocumentId(folder)
        val romsFolder = DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$ROMS")
        settingsRepository.includeRomSearchDirectory(romsFolder)
        organizeSaves()
        organizeBios()

        setUpThreeDs()
        hideImagesFromGallery()
        return SetupResult.Success to organized
    }

    /**
     * Creates BIOS's console folders and points the DS and DSi BIOS settings to BIOS/DS and BIOS/DSi, unless
     * they point elsewhere. Earlier versions kept everything in BIOS itself: its DS or DSi files (told apart
     * by bios7.bin's size) and gba_bios.bin move into their console's folder; a file already there is kept.
     * Runs on a background thread, at setup and when the game list opens.
     */
    fun organizeBios() {
        val folder = settingsRepository.getPomegradeFolder() ?: return
        if (!hasFileAccess(context)) return
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        val bios = pathOf("$treeId/$BIOS")
        BIOS_CONSOLES.forEach { File(bios, it).mkdirs() }

        val biosUri = DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$BIOS")
        fun consoleUri(console: String) = DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$BIOS/$console")
        val bios7Size = File(bios, "bios7.bin").takeIf { it.isFile }?.length()

        val dsBios = settingsRepository.getDsBiosDirectory()
        if (dsBios == null || dsBios == biosUri) {
            if (bios7Size == DS_BIOS7_SIZE) moveInto(bios, BIOS_DS, DS_BIOS_FILES)
            settingsRepository.setDsBiosDirectory(consoleUri(BIOS_DS))
        }
        val dsiBios = settingsRepository.getDsiBiosDirectory()
        if (dsiBios == null || dsiBios == biosUri) {
            if (bios7Size == DSI_BIOS7_SIZE) moveInto(bios, BIOS_DSI, DSI_BIOS_FILES)
            settingsRepository.setDsiBiosDirectory(consoleUri(BIOS_DSI))
        }
        moveInto(bios, BIOS_GBA, listOf(GBA_BIOS_FILE))
        // the GameCube boot ROM's region folders, so where to put IPL.bin is plain
        ConsoleSystemFiles.GC_REGIONS.forEach { File(File(bios, GC_WII), it).mkdirs() }
        ConsoleSystemFiles.import(bios, pathOf(treeId))
    }

    /**
     * Each console's saves in its own folder (owner's decision, 13 October): moves the DS and GBA saves kept
     * next to their games into DS and GBA ([ConsoleSavesMover]), then points the DS saves setting to DS,
     * unless the user set another folder in it. Runs on a background thread, at setup and when the game list
     * opens.
     */
    fun organizeSaves() {
        val folder = settingsRepository.getPomegradeFolder() ?: return
        if (!hasFileAccess(context)) return
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        ConsoleSavesMover.moveAll(pathOf("$treeId/$ROMS"), pathOf("$treeId/$DS_SAVES"), pathOf("$treeId/$GBA_SAVES"))
        if (settingsRepository.saveNextToRomFile() || settingsRepository.getSaveFileDirectory() == null) {
            settingsRepository.keepGameFilesInConsoleFolder(DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$DS_SAVES"))
        }
    }

    private fun moveInto(bios: File, console: String, names: List<String>) {
        for (name in names) {
            val file = File(bios, name)
            val target = File(File(bios, console), name)
            if (file.isFile && !target.exists()) file.renameTo(target)
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
        if (restored) romsRepository.reloadRoms()
        return restored
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
     * Keeps the image files the emulators write out of the phone's gallery: a `.nomedia` file in the
     * DS texture folder (Textures/<game code>/, its dumps in dump/), the save state previews and the 3DS
     * core's dump folder (dump/textures/<title id>/), which Android's media scanner otherwise lists as
     * photos (the owner, 8 October: the dumped textures filled the gallery). A `.nomedia` hides the
     * folder's sub-folders too; files already listed leave the gallery when it next rescans. Each game's
     * textures stay in a folder of their own, as both cores write them.
     */
    fun hideImagesFromGallery() {
        val root = path() ?: return
        if (!hasFileAccess(context)) return
        val threeDs = runCatching { documentIdOf(context, PermissionsHandler.citraDirectory) }.getOrNull()?.let { pathOf(it) }
        val folders = listOfNotNull(File(root, TEXTURES), File(root, SAVE_STATE_PREVIEWS), threeDs?.let { File(it, "dump") })
        for (dir in folders) {
            runCatching {
                if (dir.isDirectory || dir.mkdirs()) File(dir, ".nomedia").createNewFile()
            }
        }
    }

    /** The games of [roms] not in Roms yet. */
    fun gamesToOrganize(roms: List<Rom>): List<Rom> {
        val romsId = romsDocumentId(settingsRepository.getPomegradeFolder()) ?: return emptyList()
        return roms.filter { rom ->
            val id = documentIdOf(context, rom.uri)
            id != null && !id.startsWith("$romsId/")
        }
    }

    /**
     * Moves each game of [roms] not in Roms yet into Roms/<file name>/, its DS or GBA saves into DS or
     * GBA, and updates its entry (settings, last played and play time follow it). A game that
     * can't be moved stays where it is. Runs on a background thread.
     */
    fun organize(roms: List<Rom>, onProgress: (String) -> Unit): OrganizeResult {
        val folder = settingsRepository.getPomegradeFolder() ?: return OrganizeResult(0, emptyList())
        val treeId = DocumentsContract.getTreeDocumentId(folder)
        val romsDir = pathOf("$treeId/$ROMS")
        var moved = 0
        val failed = ArrayList<String>()
        for (rom in gamesToOrganize(roms)) {
            onProgress(rom.name.ifEmpty { rom.fileName })
            if (moveGame(rom, folder, treeId, romsDir)) moved++ else failed.add(rom.fileName)
        }
        return OrganizeResult(moved, failed)
    }

    private fun moveGame(rom: Rom, folder: Uri, treeId: String, romsDir: File): Boolean {
        val romId = documentIdOf(context, rom.uri) ?: return false
        val source = pathOf(romId)
        if (!source.isFile) return false

        // the files that go with it, found where the settings keep them now
        val baseName = source.nameWithoutExtension
        val companions = ArrayList<File>()
        if (rom.platform == RomPlatform.NDS) {
            fileDirectory { settingsRepository.getSaveFileDirectory(rom) }?.let { companions.add(File(it, "$baseName.sav")) }
            fileDirectory { settingsRepository.getSaveStateDirectory(rom) }?.let { dir ->
                for (slot in 0 until SAVE_STATE_SLOTS) companions.add(File(dir, "$baseName.ml$slot"))
            }
        } else if (rom.platform == RomPlatform.GBA) {
            // kept next to the game before (GbaEmulatorActivity): its save, its states and their previews
            val dir = source.parentFile
            companions.add(File(dir, "$baseName.sav"))
            for (slot in 0 until GBA_STATE_SLOTS) {
                companions.add(File(dir, "$baseName.ss$slot"))
                companions.add(File(dir, "$baseName.ss$slot.png"))
            }
        }

        val gameDirName = GameFolderMover.move(source, emptyList(), romsDir) ?: return false
        // its saves into its console's folder (a file already there is left where it is)
        val savesDir = pathOf("$treeId/" + if (rom.platform == RomPlatform.GBA) GBA_SAVES else DS_SAVES)
        if (companions.isNotEmpty() && (savesDir.isDirectory || savesDir.mkdirs())) {
            companions.filter { it.isFile && it.parentFile?.canonicalFile != savesDir.canonicalFile }
                .forEach { GameFolderMover.moveFile(it, File(savesDir, it.name)) }
        }

        val gameId = "$treeId/$ROMS/$gameDirName"
        val newUri = DocumentsContract.buildDocumentUriUsingTree(folder, "$gameId/${source.name}")
        val parentUri = DocumentsContract.buildDocumentUriUsingTree(folder, gameId)
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
