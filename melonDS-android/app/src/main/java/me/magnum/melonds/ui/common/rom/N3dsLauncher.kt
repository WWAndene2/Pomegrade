package me.magnum.melonds.ui.common.rom

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.Settings as AndroidSettings
import android.widget.Toast
import androidx.preference.PreferenceManager
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.activities.EmulationActivity
import org.citra.citra_emu.features.settings.model.Settings
import org.citra.citra_emu.utils.CitraDirectoryHelper
import org.citra.citra_emu.utils.DirectoryInitialization
import org.citra.citra_emu.utils.PermissionsHandler

/**
 * Starts 3DS games in the Azahar core, and sets up the folder it needs first (saves, save states,
 * system files), from Pomegrade rather than Azahar's own home screen.
 */
object N3dsLauncher {

    fun isSupported(): Boolean = CitraApplication.isSupported

    fun isFolderReady(context: Context): Boolean = PermissionsHandler.hasWriteAccess(context)

    /**
     * The 3DS core (Azahar's standard build) reads and writes its folder through file paths, which
     * needs "All files access" from Android 11, the storage permission on Android 10: what Azahar's
     * own setup asks for.
     */
    fun hasFileAccess(context: Context): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Environment.isExternalStorageManager()
        } else {
            context.checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED
        }
    }

    /** Android 11+: the system screen where "All files access" is granted to Pomegrade. */
    fun allFilesAccessSettings(context: Context): Intent =
        Intent(AndroidSettings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.fromParts("package", context.packageName, null))

    /** The same, as a list of every app, where the app's own screen doesn't exist (some phones). */
    fun allFilesAccessSettingsList(): Intent = Intent(AndroidSettings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION)

    /**
     * Makes [folder] (picked with the system's folder picker) the 3DS core's folder. Returns false
     * if it can't be used: the core works with file paths, so the folder must be on the phone's
     * storage (not in a cloud or another app's provider).
     */
    fun setUpFolder(context: Context, folder: Uri): Boolean {
        val path = try {
            NativeLibrary.getNativePath(folder)
        } catch (e: Exception) {
            ""
        }
        if (path.isEmpty()) {
            return false
        }

        try {
            val flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
            context.contentResolver.takePersistableUriPermission(folder, flags)
        } catch (e: SecurityException) {
            return false
        }
        CitraDirectoryHelper.initializeCitraDirectory(folder)
        // Azahar's own first-time setup would ask for the folder again if its home screen opens
        PreferenceManager.getDefaultSharedPreferences(context).edit()
            .putBoolean(Settings.PREF_FIRST_APP_LAUNCH, false)
            .apply()
        return DirectoryInitialization.areCitraDirectoriesReady()
    }

    fun launch(activity: Activity, rom: Rom) {
        if (!isSupported()) {
            Toast.makeText(activity, R.string.three_ds_unsupported, Toast.LENGTH_LONG).show()
            return
        }

        val intent = Intent(activity, EmulationActivity::class.java).apply {
            action = Intent.ACTION_VIEW
            data = rom.uri
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        activity.startActivity(intent)
    }
}
