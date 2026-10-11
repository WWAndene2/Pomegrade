package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.util.Log
import android.widget.Toast
import me.magnum.melonds.R
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.impl.SwitchFolder
import me.magnum.melonds.impl.SwitchSystemFiles
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivityArgs
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
            prepare(activity)
            activity.startActivity(launchIntent(activity, rom))
        }
    }

    /**
     * Before opening one of Eden's screens: its data in the Pomegrade folder's Switch ([SwitchFolder]), then the
     * keys and firmware of BIOS/Switch ([SwitchSystemFiles]), so the NCAs go to that NAND. A step that fails is
     * told to the owner (Eden opens anyway: what it already has may be enough), otherwise Eden's own error
     * would be the only sign, and it doesn't name the file at fault.
     */
    suspend fun prepare(context: Context) {
        YuzuApplication.start() // on the main thread, as Eden's own screens start it
        val appContext = context.applicationContext
        val failure = withContext(Dispatchers.IO) {
            // both steps run whatever the other does; the first failure is the one told
            val folder = runCatching { SwitchFolder.setUp(appContext) }.exceptionOrNull()?.let { R.string.switch_folder_failed to it }
            val systemFiles = runCatching { SwitchSystemFiles.import(appContext) }.exceptionOrNull()?.let { R.string.switch_system_files_failed to it }
            folder ?: systemFiles
        }
        failure?.let { (message, error) ->
            Log.w(TAG, "Switch core not prepared", error)
            Toast.makeText(appContext, appContext.getString(message, error.localizedMessage ?: error.javaClass.simpleName), Toast.LENGTH_LONG).show()
        }
    }

    private const val TAG = "SwitchLauncher"

    override val unsupportedMessage get() = R.string.switch_unsupported

    override fun launchIntent(context: Context, rom: Rom): Intent = Intent(context, EmulationActivity::class.java).apply {
        action = Intent.ACTION_VIEW
        data = rom.uri
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
    }

    /** One section of Eden's settings, as its home screen opens them. */
    fun settingsIntent(context: Context, section: Settings.MenuTag): Intent =
        Intent(context, SettingsActivity::class.java).putExtras(SettingsActivityArgs(game = null, menuTag = section).toBundle())

    /** Eden's home screen: its setup (keys, firmware), its game list and its settings. */
    fun homeIntent(context: Context): Intent = Intent(context, MainActivity::class.java)
}
