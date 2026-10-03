package me.magnum.melonds.ui.common

import android.Manifest
import android.content.ActivityNotFoundException
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import dagger.hilt.EntryPoint
import dagger.hilt.InstallIn
import dagger.hilt.android.EntryPointAccessors
import dagger.hilt.components.SingletonComponent
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.magnum.melonds.R
import me.magnum.melonds.domain.repositories.RomsRepository
import me.magnum.melonds.impl.PomegradeFolder

/**
 * Sets up the Pomegrade folder (see [PomegradeFolder]): explains it, gets "All files access" from
 * Android, lets the user pick the folder, then moves the games into it. Create it while the
 * activity is being created (it registers for activity results).
 */
class PomegradeFolderSetupDelegate(private val activity: ComponentActivity) {

    @EntryPoint
    @InstallIn(SingletonComponent::class)
    interface Dependencies {
        fun pomegradeFolder(): PomegradeFolder
        fun romsRepository(): RomsRepository
    }

    private val dependencies by lazy {
        EntryPointAccessors.fromApplication(activity.applicationContext, Dependencies::class.java)
    }
    private val pomegradeFolder get() = dependencies.pomegradeFolder()

    // what to do once done (true: the folder is set up and usable). Lost if the activity is
    // recreated meanwhile: what was set up stays, the user taps again
    private var onDone: ((Boolean) -> Unit)? = null

    private val allFilesAccessLauncher = activity.registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        afterFileAccess(PomegradeFolder.hasFileAccess(activity))
    }
    private val storagePermissionLauncher = activity.registerForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
        afterFileAccess(granted)
    }
    private val folderLauncher: ActivityResultLauncher<Uri?> = activity.registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { folder ->
        if (folder == null) finish(false) else setUp(folder)
    }

    fun isSetUp(): Boolean = pomegradeFolder.isSetUp() && PomegradeFolder.hasFileAccess(activity)

    fun start(onDone: (Boolean) -> Unit) {
        this.onDone = onDone
        when {
            isSetUp() -> finish(true)
            // set up before, the file access was withdrawn since
            pomegradeFolder.isSetUp() -> requestFileAccess()
            else -> AlertDialog.Builder(activity)
                .setTitle(R.string.pomegrade_folder_title)
                .setMessage(R.string.pomegrade_folder_message)
                .setPositiveButton(R.string.pomegrade_folder_choose) { _, _ -> requestFileAccess() }
                .setNegativeButton(R.string.pomegrade_folder_later) { _, _ -> finish(false) }
                .setOnCancelListener { finish(false) }
                .show()
        }
    }

    private fun requestFileAccess() {
        if (PomegradeFolder.hasFileAccess(activity)) {
            afterFileAccess(true)
            return
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Toast.makeText(activity, R.string.pomegrade_folder_file_access_hint, Toast.LENGTH_LONG).show()
            try {
                allFilesAccessLauncher.launch(Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.fromParts("package", activity.packageName, null)))
            } catch (e: ActivityNotFoundException) {
                // some phones only have the list of every app
                allFilesAccessLauncher.launch(Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION))
            }
        } else {
            storagePermissionLauncher.launch(Manifest.permission.WRITE_EXTERNAL_STORAGE)
        }
    }

    private fun afterFileAccess(granted: Boolean) {
        when {
            !granted -> {
                Toast.makeText(activity, R.string.pomegrade_folder_file_access_denied, Toast.LENGTH_LONG).show()
                finish(false)
            }
            pomegradeFolder.isSetUp() -> finish(true)
            else -> folderLauncher.launch(null)
        }
    }

    private fun setUp(folder: Uri) {
        val progress = AlertDialog.Builder(activity)
            .setTitle(R.string.pomegrade_folder_title)
            .setMessage(R.string.pomegrade_folder_setting_up)
            .setCancelable(false)
            .show()
        activity.lifecycleScope.launch {
            val roms = dependencies.romsRepository().getRoms().first()
            val (result, organized) = withContext(Dispatchers.IO) {
                pomegradeFolder.setUp(folder, roms) { game ->
                    activity.runOnUiThread { progress.setMessage(activity.getString(R.string.pomegrade_folder_moving, game)) }
                }
            }
            progress.dismiss()
            when (result) {
                PomegradeFolder.SetupResult.Success -> {
                    organized?.let { report(it) }
                    finish(true)
                }
                PomegradeFolder.SetupResult.UnsupportedFolder -> AlertDialog.Builder(activity)
                    .setTitle(R.string.pomegrade_folder_title)
                    .setMessage(R.string.pomegrade_folder_invalid)
                    .setPositiveButton(R.string.pomegrade_folder_choose) { _, _ -> folderLauncher.launch(null) }
                    .setNegativeButton(R.string.cancel) { _, _ -> finish(false) }
                    .setOnCancelListener { finish(false) }
                    .show()
                PomegradeFolder.SetupResult.Failed -> {
                    Toast.makeText(activity, R.string.pomegrade_folder_failed, Toast.LENGTH_LONG).show()
                    finish(false)
                }
            }
        }
    }

    /** What organizing the games did: how many moved, and the ones that couldn't be. */
    fun report(result: PomegradeFolder.OrganizeResult) {
        if (result.moved > 0) {
            val text = activity.resources.getQuantityString(R.plurals.pomegrade_folder_moved, result.moved, result.moved)
            Toast.makeText(activity, text, Toast.LENGTH_LONG).show()
        }
        if (result.failed.isNotEmpty()) {
            AlertDialog.Builder(activity)
                .setTitle(R.string.pomegrade_folder_not_moved_title)
                .setMessage(activity.getString(R.string.pomegrade_folder_not_moved, result.failed.joinToString("\n")))
                .setPositiveButton(R.string.ok, null)
                .show()
        }
    }

    private fun finish(success: Boolean) {
        val callback = onDone
        onDone = null
        callback?.invoke(success)
    }
}
