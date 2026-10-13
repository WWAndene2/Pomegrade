package me.magnum.melonds.ui.common

import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.magnum.melonds.R
import me.magnum.melonds.impl.PunicaFolderRename
import kotlin.system.exitProcess

/**
 * The screens of the folder's rename to Punica ([PunicaFolderRename]): explains it, renames the folder, has
 * the user confirm the new folder in the system's picker (Android's only way to give a folder's access),
 * then restarts the app. Not confirming puts the old name back. Create it while the activity is being
 * created (it registers for activity results).
 */
class PunicaFolderRenameDelegate(private val activity: ComponentActivity) {

    private val folderLauncher: ActivityResultLauncher<Uri?> = activity.registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { folder ->
        if (folder == null) revert() else complete(folder)
    }

    /** The folder is still named Pomegrade, or its rename was interrupted. */
    fun isNeeded(): Boolean = PunicaFolderRename.isPending(activity) || PunicaFolderRename.isNeeded(activity)

    fun start() {
        if (PunicaFolderRename.isPending(activity)) {
            // renamed, not confirmed yet (the app stopped meanwhile): straight to the confirmation
            askToConfirm()
            return
        }
        AlertDialog.Builder(activity)
            .setTitle(R.string.punica_folder_rename_title)
            .setMessage(R.string.punica_folder_rename_message)
            .setPositiveButton(R.string.punica_folder_rename_ok) { _, _ -> rename() }
            .setNegativeButton(R.string.punica_folder_rename_later, null)
            .show()
    }

    private fun rename() {
        activity.lifecycleScope.launch {
            when (withContext(Dispatchers.IO) { PunicaFolderRename.rename(activity) }) {
                PunicaFolderRename.RenameResult.Renamed -> askToConfirm()
                PunicaFolderRename.RenameResult.TargetExists -> message(R.string.punica_folder_rename_exists)
                PunicaFolderRename.RenameResult.Failed -> message(R.string.punica_folder_rename_failed)
            }
        }
    }

    private fun askToConfirm() {
        AlertDialog.Builder(activity)
            .setTitle(R.string.punica_folder_rename_title)
            .setMessage(R.string.punica_folder_rename_confirm)
            .setPositiveButton(R.string.punica_folder_rename_confirm_ok) { _, _ -> folderLauncher.launch(PunicaFolderRename.newFolderUri(activity)) }
            .setNegativeButton(R.string.cancel) { _, _ -> revert() }
            .setCancelable(false)
            .show()
    }

    private fun complete(folder: Uri) {
        activity.lifecycleScope.launch {
            val done = withContext(Dispatchers.IO) { runCatching { PunicaFolderRename.complete(activity, folder) }.getOrDefault(false) }
            if (!done) {
                // another folder was picked: asked again, the folder stays renamed meanwhile
                Toast.makeText(activity, R.string.punica_folder_rename_wrong, Toast.LENGTH_LONG).show()
                askToConfirm()
                return@launch
            }
            Toast.makeText(activity, R.string.punica_folder_rename_done, Toast.LENGTH_LONG).show()
            restart()
        }
    }

    private fun revert() {
        activity.lifecycleScope.launch {
            val reverted = withContext(Dispatchers.IO) { PunicaFolderRename.revert(activity) }
            if (reverted) {
                Toast.makeText(activity, R.string.punica_folder_rename_reverted, Toast.LENGTH_LONG).show()
            } else {
                // the old name couldn't come back (a new Pomegrade folder appeared): the rename is the way out
                askToConfirm()
            }
        }
    }

    private fun message(text: Int) {
        AlertDialog.Builder(activity)
            .setTitle(R.string.punica_folder_rename_title)
            .setMessage(text)
            .setPositiveButton(R.string.ok, null)
            .show()
    }

    // every screen, repository and core reads the folder again from the start
    private fun restart() {
        val intent = activity.packageManager.getLaunchIntentForPackage(activity.packageName) ?: return
        activity.startActivity(intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK))
        exitProcess(0)
    }
}
