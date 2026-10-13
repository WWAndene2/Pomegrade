package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.widget.Toast
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.ui.romlist.RomListActivity

/**
 * Starts a game in one of the cores Pomegrade imports (3DS, GBA, Switch, GameCube and Wii). DS games are not here: they run
 * in melonDS's own EmulatorActivity, after melonDS's BIOS and firmware checks.
 */
interface ConsoleLauncher {

    /** Whether this device can run the core (some need a 64-bit device). */
    fun isSupported(): Boolean = true

    /** Whether the core has what it needs before launching (the Pomegrade folder and its file access). */
    fun isReady(context: Context): Boolean = true

    /** The intent that runs [rom] in the core, or null when it can't run it. */
    fun launchIntent(context: Context, rom: Rom): Intent?

    /** Shown when the device can't run the core. */
    val unsupportedMessage: Int? get() = null

    /** Shown when [launchIntent] gives no intent. */
    val launchFailedMessage: Int? get() = null

    fun launch(activity: Activity, rom: Rom) {
        val intent = if (isSupported()) launchIntent(activity, rom) else null
        if (intent == null) {
            val message = if (isSupported()) launchFailedMessage else unsupportedMessage
            message?.let { Toast.makeText(activity, it, Toast.LENGTH_LONG).show() }
            return
        }
        activity.startActivity(intent)
    }

    companion object {
        /**
         * The intent of a home-screen shortcut to [rom], for every console: it opens the game list, which
         * starts [rom] as a tap in it would (the folder checks and each core's preparation included).
         */
        fun gameListShortcutIntent(context: Context, rom: Rom): Intent =
            Intent(context, RomListActivity::class.java)
                .setAction(RomListActivity.ACTION_LAUNCH_ROM)
                .putExtra(RomListActivity.EXTRA_ROM_URI, rom.uri.toString())

        /** The launcher of [platform]'s core, or null for DS games. */
        fun of(platform: RomPlatform): ConsoleLauncher? = when (platform) {
            RomPlatform.NDS -> null
            RomPlatform.N3DS -> N3dsLauncher
            RomPlatform.GBA -> GbaLauncher
            RomPlatform.SWITCH -> SwitchLauncher
            RomPlatform.GC_WII -> GcWiiLauncher
        }
    }
}
