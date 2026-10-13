package me.magnum.melonds.ui.common.rom

import android.content.Context
import androidx.appcompat.app.AlertDialog
import me.magnum.melonds.R
import me.magnum.melonds.impl.TexturePackInstaller

/** Tells what a texture pack's install did (TexturePackInstaller), from the game's settings or the game sheet. */
object TexturePackResultDialog {

    fun show(context: Context, result: TexturePackInstaller.Result) {
        val message = when (result) {
            is TexturePackInstaller.Result.Installed -> {
                val installed = context.getString(R.string.texture_pack_installed, result.files, result.folder.path)
                if (result.enableHint) installed + "\n\n" + context.getString(R.string.texture_pack_enable_hint) else installed
            }
            TexturePackInstaller.Result.NoGameId -> context.getString(R.string.texture_pack_no_game_id)
            TexturePackInstaller.Result.NoFolder -> context.getString(R.string.texture_pack_no_folder)
            TexturePackInstaller.Result.Empty -> context.getString(R.string.texture_pack_empty)
            is TexturePackInstaller.Result.Failed -> context.getString(R.string.texture_pack_failed, result.error.localizedMessage ?: result.error.javaClass.simpleName)
        }
        AlertDialog.Builder(context)
            .setTitle(R.string.texture_pack_install)
            .setMessage(message)
            .setPositiveButton(R.string.ok, null)
            .show()
    }
}
