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
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.utils.CitraDirectoryHelper
import org.citra.citra_emu.utils.PermissionsHandler
import java.io.File

/**
 * The Pomegrade folder: one folder, picked once, where Pomegrade keeps the games and their files
 * for every console:
 *
 * ```
 * Pomegrade/
 * ├── Roms/<game>/   each game's ROM, with its DS saves and save states next to it
 * ├── BIOS/          DS/DSi BIOS and firmware (when none was set up elsewhere)
 * └── 3DS/           the 3DS core's folder (Azahar: 3DS saves, save states, system files)
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
) {

    companion object {
        const val ROMS = "Roms"
        const val BIOS = "BIOS"
        const val N3DS = "3DS"
        private const val STORAGE_PROVIDER = "com.android.externalstorage.documents"
        // DS save states: <ROM name>.ml0 (quick save) to .ml9 (see FileSystemSaveStatesRepository)
        private const val SAVE_STATE_SLOTS = 10

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
        for (name in listOf(ROMS, BIOS, N3DS)) {
            val dir = File(root, name)
            if (!dir.isDirectory && !dir.mkdirs()) {
                return SetupResult.Failed to null
            }
        }
        settingsRepository.setPomegradeFolder(folder)

        // games first, while the settings still say where their saves are now
        val organized = organize(roms, onProgress)

        val treeId = DocumentsContract.getTreeDocumentId(folder)
        val romsFolder = DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$ROMS")
        settingsRepository.includeRomSearchDirectory(romsFolder)
        settingsRepository.keepGameFilesNextToRom()
        val bios = DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$BIOS")
        if (settingsRepository.getDsBiosDirectory() == null) settingsRepository.setDsBiosDirectory(bios)
        if (settingsRepository.getDsiBiosDirectory() == null) settingsRepository.setDsiBiosDirectory(bios)

        // the 3DS core: its folder here, unless it was already set up elsewhere (its data stays there)
        if (!PermissionsHandler.hasWriteAccess(context)) {
            CitraDirectoryHelper.initializeCitraDirectory(DocumentsContract.buildDocumentUriUsingTree(folder, "$treeId/$N3DS"))
            // Azahar's own first-time setup would ask for a folder again if its home screen opens
            PreferenceManager.getDefaultSharedPreferences(context).edit()
                .putBoolean(Settings.PREF_FIRST_APP_LAUNCH, false)
                .apply()
        }
        return SetupResult.Success to organized
    }

    /** Set up, with the file access moving games needs. */
    fun canOrganize(): Boolean = isSetUp() && hasFileAccess(context)

    /** The games of [roms] not in Roms yet. */
    fun gamesToOrganize(roms: List<Rom>): List<Rom> {
        val romsId = romsDocumentId(settingsRepository.getPomegradeFolder()) ?: return emptyList()
        return roms.filter { rom ->
            val id = documentIdOf(context, rom.uri)
            id != null && !id.startsWith("$romsId/")
        }
    }

    /**
     * Moves each game of [roms] not in Roms yet into Roms/<file name>/, with its DS save and save
     * states, and updates its entry (settings, last played and play time follow it). A game that
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
        }

        val gameDirName = GameFolderMover.move(source, companions, romsDir) ?: return false

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
