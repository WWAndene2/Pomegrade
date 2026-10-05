package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.widget.Toast
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.impl.PomegradeFolder
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.activities.EmulationActivity
import org.citra.citra_emu.utils.PermissionsHandler

/**
 * Starts 3DS games in the Azahar core. Its folder (saves, save states, system files) is the
 * Pomegrade folder's 3DS, set up by PomegradeFolderSetupDelegate.
 */
object N3dsLauncher {

    fun isSupported(): Boolean = CitraApplication.isSupported

    /** The 3DS core has its folder and the file access it reads it with. */
    fun isReady(context: Context): Boolean =
        PomegradeFolder.hasFileAccess(context) && PermissionsHandler.hasWriteAccess(context)

    fun launch(activity: Activity, rom: Rom) {
        if (!isSupported()) {
            Toast.makeText(activity, R.string.three_ds_unsupported, Toast.LENGTH_LONG).show()
            return
        }

        // the remake's test bench, when Pomegrade/3DS/bench holds mods (temporary: N3dsModBench)
        if (N3dsModBench.isRequested(activity)) {
            N3dsModBench.start(activity, rom)
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
