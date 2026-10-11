package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.widget.Toast
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform

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

    /** The intent a home-screen shortcut opens, or null when the core's screen can't be opened from one. */
    fun shortcutIntent(context: Context, rom: Rom): Intent? = launchIntent(context, rom)

    /** Shown when the device can't run the core. */
    val unsupportedMessage: Int? get() = null

    /** Shown when [launchIntent] gives no intent. */
    val launchFailedMessage: Int? get() = null

    /** Shown when [shortcutIntent] gives no intent. */
    val noShortcutMessage: Int? get() = null

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
