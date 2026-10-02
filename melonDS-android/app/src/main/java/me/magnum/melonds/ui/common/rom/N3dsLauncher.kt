package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.content.Intent
import android.widget.Toast
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.activities.EmulationActivity
import org.citra.citra_emu.ui.main.MainActivity
import org.citra.citra_emu.utils.PermissionsHandler

/**
 * Starts 3DS games in the Azahar core.
 */
object N3dsLauncher {

    fun launch(activity: Activity, rom: Rom) {
        if (!CitraApplication.isSupported) {
            Toast.makeText(activity, R.string.three_ds_unsupported, Toast.LENGTH_LONG).show()
            return
        }

        // Azahar needs its user folder (saves, system files) before it can run anything. Its
        // home screen walks through that setup the first time.
        if (!PermissionsHandler.hasWriteAccess(activity)) {
            Toast.makeText(activity, R.string.three_ds_setup_required, Toast.LENGTH_LONG).show()
            activity.startActivity(Intent(activity, MainActivity::class.java))
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
