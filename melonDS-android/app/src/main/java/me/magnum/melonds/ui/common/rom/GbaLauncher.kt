package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.widget.Toast
import io.github.wwandene2.pomegrade.gba.GbaEmulatorActivity
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.impl.PomegradeFolder
import java.io.File

/**
 * Starts GBA games in the mGBA core. The core reads the game, and writes its save and save states
 * next to it, by file path, so it needs the Pomegrade folder's file access. A BIOS dumped from the
 * owner's console, `gba_bios.bin` in the Pomegrade folder's BIOS, is used when present; mGBA's own
 * otherwise.
 */
object GbaLauncher {

    const val BIOS_FILE = "gba_bios.bin"

    fun isReady(context: Context): Boolean = PomegradeFolder.hasFileAccess(context)

    fun launch(activity: Activity, rom: Rom) {
        val intent = launchIntent(activity, rom)
        if (intent == null) {
            Toast.makeText(activity, R.string.gba_not_a_file, Toast.LENGTH_LONG).show()
            return
        }
        activity.startActivity(intent)
    }

    /** The intent that runs [rom] in the GBA core, or null when the game isn't a file of the phone's storage. */
    fun launchIntent(context: Context, rom: Rom): Intent? {
        val documentId = PomegradeFolder.documentIdOf(context, rom.uri) ?: return null
        val path = PomegradeFolder.pathOf(documentId)
        if (!path.isFile) return null
        val bios = PomegradeFolder.subFolder(context, PomegradeFolder.BIOS)?.let { File(it, BIOS_FILE) }?.takeIf { it.isFile }
        return GbaEmulatorActivity.intent(context, path.absolutePath, rom.config.customName ?: rom.name, bios?.absolutePath)
    }
}
