// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.concurrent.atomic.AtomicBoolean
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.utils.PermissionsHandler.hasWriteAccess

/**
 * A service that spawns its own thread in order to copy several binary and shader files
 * from the Citra APK to the external file system.
 */
object DirectoryInitialization {
    /**
     * Pomegrade: where the emulator's own files are (config, log, gpu_drivers, GameSettings) in the user folder [root]:
     * its Core folder in the Punica folder's layout (melonDS-android's PunicaLayout, told by Core and System folders,
     * as FileUtil::SetUserPath does), else [root] itself.
     */
    fun punicaCore(root: DocumentFile): DocumentFile {
        val core = root.findFile("Core")
        return if (core != null && core.isDirectory && root.findFile("System")?.isDirectory == true) core else root
    }

    private const val SYS_DIR_VERSION = "sysDirectoryVersion"

    @Volatile
    private var directoryState: DirectoryInitializationState? = null
    var userPath: String? = null
    val internalUserPath: String
        get() = CitraApplication.appContext.filesDir.canonicalPath
    private val isCitraDirectoryInitializationRunning = AtomicBoolean(false)

    val context: Context get() = CitraApplication.appContext

    /** Pomegrade: a user folder change that came while a game ran, applied by the next [start] */
    @Volatile
    private var userFolderChangePending = false

    /**
     * Pomegrade: the user folder changed under the core (the Punica app moved it to its layout, renamed it or
     * picked another one): its paths, the SAF tree, the log, the settings and the play times follow the current
     * folder. Callable from any thread, before the core started (nothing to do then: [start] reads the current
     * folder). While a game runs, nothing is pulled from under it (its saves, states and log stay where they
     * are): the change is kept and applied by the next [start], which every game launch calls.
     */
    @JvmStatic
    @Synchronized
    fun onUserFolderChanged() {
        if (!CitraApplication.isStarted) {
            return
        }
        if (NativeLibrary.isRunning()) {
            userFolderChangePending = true
            return
        }
        userFolderChangePending = false
        directoryState = null
        if (start() == DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED) {
            NativeLibrary.playTimeManagerReload()
        }
    }

    @JvmStatic
    @Synchronized
    fun start(): DirectoryInitializationState? {
        if (!isCitraDirectoryInitializationRunning.compareAndSet(false, true)) {
            return null
        }
        // Pomegrade: a user folder change kept while a game ran (onUserFolderChanged)
        var reloadPlayTimes = false
        if (userFolderChangePending && !NativeLibrary.isRunning()) {
            userFolderChangePending = false
            directoryState = null
            reloadPlayTimes = true
        }

        if (directoryState != DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED) {
            directoryState = if (hasWriteAccess(context)) {
                if (setCitraUserDirectory()) {
                    CitraApplication.documentsTree.setRoot(Uri.parse(userPath))
                    NativeLibrary.resetUserPaths()
                    // Pomegrade: reopens the log at the current LogDir when logging already runs
                    NativeLibrary.createLogFile()
                    NativeLibrary.logUserDirectory(userPath.toString())
                    NativeLibrary.createConfigFile()
                    GpuDriverHelper.initializeDriverParameters()
                    DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED
                } else {
                    DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE
                }
            } else {
                DirectoryInitializationState.EXTERNAL_STORAGE_PERMISSION_NEEDED
            }
        }
        isCitraDirectoryInitializationRunning.set(false)
        if (reloadPlayTimes && directoryState == DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED) {
            NativeLibrary.playTimeManagerReload()
        }
        return directoryState
    }

    private fun deleteDirectoryRecursively(file: File) {
        if (file.isDirectory) {
            for (child in file.listFiles()!!) {
                deleteDirectoryRecursively(child)
            }
        }
        file.delete()
    }

    @JvmStatic
    fun areCitraDirectoriesReady(): Boolean =
        directoryState == DirectoryInitializationState.CITRA_DIRECTORIES_INITIALIZED

    fun resetCitraDirectoryState() {
        directoryState = null
        isCitraDirectoryInitializationRunning.compareAndSet(true, false)
    }

    val userDirectory: String?
        get() {
            checkNotNull(directoryState) {
                "DirectoryInitialization has to run at least once!"
            }
            check(!isCitraDirectoryInitializationRunning.get()) {
                "DirectoryInitialization has to finish running first!"
            }
            return userPath
        }

    fun setCitraUserDirectory(): Boolean {
        val dataPath = PermissionsHandler.citraDirectory
        if (dataPath.toString().isNotEmpty()) {
            userPath = dataPath.toString()
            android.util.Log.d("[Azahar Frontend]", "[DirectoryInitialization] User Dir: $userPath")
            return true
        }
        return false
    }

    private fun copyAsset(asset: String, output: File, overwrite: Boolean, context: Context) {
        Log.debug("[DirectoryInitialization] Copying File $asset to $output")
        try {
            if (!output.exists() || overwrite) {
                val inputStream = context.assets.open(asset)
                val outputStream = FileOutputStream(output)
                copyFile(inputStream, outputStream)
                inputStream.close()
                outputStream.close()
            }
        } catch (e: IOException) {
            Log.error("[DirectoryInitialization] Failed to copy asset file: $asset" + e.message)
        }
    }

    private fun copyAssetFolder(
        assetFolder: String,
        outputFolder: File,
        overwrite: Boolean,
        context: Context
    ) {
        Log.debug("[DirectoryInitialization] Copying Folder $assetFolder to $outputFolder")
        try {
            var createdFolder = false
            for (file in context.assets.list(assetFolder)!!) {
                if (!createdFolder) {
                    outputFolder.mkdir()
                    createdFolder = true
                }
                copyAssetFolder(
                    assetFolder + File.separator + file,
                    File(outputFolder, file),
                    overwrite,
                    context
                )
                copyAsset(
                    assetFolder + File.separator + file,
                    File(outputFolder, file),
                    overwrite,
                    context
                )
            }
        } catch (e: IOException) {
            Log.error(
                "[DirectoryInitialization] Failed to copy asset folder: $assetFolder" +
                    e.message
            )
        }
    }

    @Throws(IOException::class)
    private fun copyFile(inputStream: InputStream, outputStream: OutputStream) {
        val buffer = ByteArray(1024)
        var read: Int
        while (inputStream.read(buffer).also { read = it } != -1) {
            outputStream.write(buffer, 0, read)
        }
    }

    enum class DirectoryInitializationState {
        CITRA_DIRECTORIES_INITIALIZED,
        EXTERNAL_STORAGE_PERMISSION_NEEDED,
        CANT_FIND_EXTERNAL_STORAGE
    }
}
