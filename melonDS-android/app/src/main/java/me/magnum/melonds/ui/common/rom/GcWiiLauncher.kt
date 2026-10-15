package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.widget.Toast
import androidx.fragment.app.FragmentActivity
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import org.dolphinemu.dolphinemu.DolphinApplication
import org.dolphinemu.dolphinemu.activities.EmulationActivity
import org.dolphinemu.dolphinemu.features.settings.ui.MenuTag
import org.dolphinemu.dolphinemu.features.settings.ui.SettingsActivity

/**
 * Starts GameCube and Wii games in the Dolphin core, as Dolphin's own game list does (the game's URI as its
 * path: Dolphin reads content URIs). Dolphin's data is in the Pomegrade folder's GC-Wii (see GcWiiFolder).
 * Its emulation screen is not exported, and Dolphin starts it after its own checks: the home-screen shortcut
 * goes through the game list.
 */
object GcWiiLauncher : ConsoleLauncher {

    override fun isSupported(): Boolean = DolphinApplication.isSupported

    override val unsupportedMessage get() = R.string.gc_wii_unsupported

    override fun launch(activity: Activity, rom: Rom) {
        val fragmentActivity = activity as? FragmentActivity
        if (!isSupported() || fragmentActivity == null) {
            Toast.makeText(activity, R.string.gc_wii_unsupported, Toast.LENGTH_LONG).show()
            return
        }
        DolphinApplication.start()
        EmulationActivity.launch(fragmentActivity, rom.uri.toString(), false)
    }

    // no intent to its screen: Dolphin's EmulationActivity.launch starts it from an activity
    override fun launchIntent(context: Context, rom: Rom): Intent? = null

    /** One section of Dolphin's settings, as its home screen opens them. */
    fun openSettings(context: Context, section: MenuTag) {
        DolphinApplication.start()
        SettingsActivity.launch(context, section)
    }
}
