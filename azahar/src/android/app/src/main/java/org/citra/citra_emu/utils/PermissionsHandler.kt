// Copyright 2023-2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

package org.citra.citra_emu.utils

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Build
import android.provider.DocumentsContract
import androidx.activity.result.ActivityResultLauncher
import androidx.documentfile.provider.DocumentFile
import androidx.preference.PreferenceManager
import org.citra.citra_emu.CitraApplication

object PermissionsHandler {
    const val CITRA_DIRECTORY = "CITRA_DIRECTORY"
    val preferences: SharedPreferences =
        PreferenceManager.getDefaultSharedPreferences(CitraApplication.appContext)

    fun hasWriteAccess(context: Context): Boolean {
        try {
            if (citraDirectory.toString().isEmpty()) {
                return false
            }

            val uri = citraDirectory
            // Pomegrade: the directory may be a folder inside the app's own folder (a document
            // of a tree the app holds), whose permission is the tree's, not its own
            if (isInsideHeldTree(context, uri)) {
                val folder = DocumentFile.fromTreeUri(context, uri)
                return folder != null && folder.exists()
            }
            val takeFlags =
                Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            context.contentResolver.takePersistableUriPermission(uri, takeFlags)
            val root = DocumentFile.fromTreeUri(context, uri)
            if (root != null && root.exists()) {
                return true
            }

            context.contentResolver.releasePersistableUriPermission(uri, takeFlags)
        } catch (e: Exception) {
            // Do not use native library logging, as the native library may not be loaded yet
            android.util.Log.e(
                "PermissionsHandler",
                "Cannot check citra data directory permission, error: ${e.message}"
            )
        }
        return false
    }

    // Pomegrade: a document URI inside a tree the app holds a persisted write permission for
    private fun isInsideHeldTree(context: Context, uri: Uri): Boolean {
        if (!DocumentsContract.isDocumentUri(context, uri)) return false
        val tree = DocumentsContract.buildTreeDocumentUri(uri.authority, DocumentsContract.getTreeDocumentId(uri))
        return context.contentResolver.persistedUriPermissions.any { it.uri == tree && it.isWritePermission }
    }

    val citraDirectory: Uri
        get() {
            val directoryString = preferences.getString(CITRA_DIRECTORY, "")
            return Uri.parse(directoryString)
        }

    fun setCitraDirectory(uriString: String?) =
        preferences.edit().putString(CITRA_DIRECTORY, uriString).apply()

    fun compatibleSelectDirectory(activityLauncher: ActivityResultLauncher<Uri?>) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            activityLauncher.launch(null)
        } else {
            val initialUri = DocumentsContract.buildRootUri(
                "com.android.externalstorage.documents",
                "primary"
            )
            activityLauncher.launch(initialUri)
        }
    }
}
