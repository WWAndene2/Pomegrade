package me.magnum.melonds.ui.common.rom

import android.content.Context
import android.content.Intent
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.ui.main.MainActivity

/**
 * Starts Switch games in the Eden core, as Eden's own "open with" does (the game's URI as the intent's data).
 * The owner's keys and firmware are set up in Eden's home screen ([homeIntent]), reached from the settings.
 */
object SwitchLauncher : ConsoleLauncher {

    override fun isSupported(): Boolean = YuzuApplication.isSupported

    override val unsupportedMessage get() = R.string.switch_unsupported

    override fun launchIntent(context: Context, rom: Rom): Intent = Intent(context, EmulationActivity::class.java).apply {
        action = Intent.ACTION_VIEW
        data = rom.uri
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
    }

    /** Eden's home screen: its setup (keys, firmware), its game list and its settings. */
    fun homeIntent(context: Context): Intent = Intent(context, MainActivity::class.java)
}
