package me.magnum.melonds.ui.common.rom

import android.content.Context
import android.content.Intent
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
object N3dsLauncher : ConsoleLauncher {

    override fun isSupported(): Boolean = CitraApplication.isSupported

    /** The 3DS core has its folder and the file access it reads it with. */
    override fun isReady(context: Context): Boolean =
        PomegradeFolder.hasFileAccess(context) && PermissionsHandler.hasWriteAccess(context)

    override val unsupportedMessage get() = R.string.three_ds_unsupported

    /** The intent that runs [rom] in the 3DS core, as Azahar's own home-screen shortcuts do. */
    override fun launchIntent(context: Context, rom: Rom): Intent = Intent(context, EmulationActivity::class.java).apply {
        action = Intent.ACTION_VIEW
        data = rom.uri
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
    }
}
