package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.content.Context
import android.content.Intent
import me.magnum.melonds.R
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.impl.SwitchSystemFiles
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.ui.main.MainActivity

/**
 * Starts Switch games in the Eden core, as Eden's own "open with" does (the game's URI as the intent's data).
 * The owner's keys and firmware are taken from the Pomegrade folder's BIOS/Switch first ([SwitchSystemFiles]);
 * Eden's home screen ([homeIntent]), reached from the settings, can still install them too.
 */
object SwitchLauncher : ConsoleLauncher {

    override fun isSupported(): Boolean = YuzuApplication.isSupported

    override fun launch(activity: Activity, rom: Rom) {
        val scope = (activity as? LifecycleOwner)?.lifecycleScope
        if (!isSupported() || scope == null) {
            super.launch(activity, rom)
            return
        }
        scope.launch {
            withContext(Dispatchers.IO) { SwitchSystemFiles.import(activity) }
            activity.startActivity(launchIntent(activity, rom))
        }
    }

    override val unsupportedMessage get() = R.string.switch_unsupported

    override fun launchIntent(context: Context, rom: Rom): Intent = Intent(context, EmulationActivity::class.java).apply {
        action = Intent.ACTION_VIEW
        data = rom.uri
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
    }

    /** Eden's home screen: its setup (keys, firmware), its game list and its settings. */
    fun homeIntent(context: Context): Intent = Intent(context, MainActivity::class.java)
}
