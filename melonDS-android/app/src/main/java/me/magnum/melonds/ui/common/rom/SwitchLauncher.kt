package me.magnum.melonds.ui.common.rom

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.provider.DocumentsContract
import android.util.Log
import android.widget.Toast
import androidx.documentfile.provider.DocumentFile
import androidx.preference.PreferenceManager
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
import org.yuzu.yuzu_emu.features.DocumentProvider
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivityArgs
import org.yuzu.yuzu_emu.features.settings.ui.SettingsSubscreen
import org.yuzu.yuzu_emu.features.settings.ui.SettingsSubscreenActivity
import org.yuzu.yuzu_emu.features.settings.ui.SettingsSubscreenActivityArgs
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GameHelper
import org.yuzu.yuzu_emu.utils.Log as EdenLog

/**
 * Starts Switch games in the Eden core, as Eden's own "open with" does (the game's URI as the intent's data).
 * The owner's keys and firmware are taken from the Pomegrade folder's BIOS/Switch first ([SwitchSystemFiles]);
 * Eden's data manager, reached from the settings ([toolIntent]), can still install them too.
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
        val appContext = context.applicationContext
        // Eden's data into Pomegrade/Switch before Eden reads its folder; once started, at the next app start
        if (!YuzuApplication.isStarted) withContext(Dispatchers.IO) { SwitchFolder.moveUserData(appContext) }
        YuzuApplication.start() // on the main thread, as Eden's own screens start it
        val failure = withContext(Dispatchers.IO) {
            // both steps run whatever the other does; the first failure is the one told
            val folder = runCatching { SwitchFolder.setUp(appContext) }.exceptionOrNull()?.let { R.string.switch_folder_failed to it }
            val systemFiles = runCatching { SwitchSystemFiles.import(appContext) }.exceptionOrNull()?.let { R.string.switch_system_files_failed to it }
            folder ?: systemFiles
        }
        // Eden's first-time setup (keys, firmware, games folder) is Punica's: the keys and firmware come from
        // BIOS/Switch, the games from Punica's list. Its key is the same as Azahar's (FirstApplicationLaunch,
        // both in the app's default preferences), which the 3DS folder setup already sets
        PreferenceManager.getDefaultSharedPreferences(appContext).edit()
            .putBoolean(Settings.PREF_FIRST_APP_LAUNCH, false)
            .apply()
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

    /**
     * Whether the GPU is an Adreno (Qualcomm's kernel device): Eden's Turnip/Freedreno settings apply to it only.
     * Read here rather than through Eden's GpuDriverHelper, whose check needs its native library loaded.
     */
    fun isAdrenoGpu(): Boolean = java.io.File("/dev/kgsl-3d0").exists()

    /**
     * A game's options page (Eden's GamePropertiesFragment: per-game settings, add-ons, cheats, saves, driver), as
     * its game list opens it; null when Eden cannot read the game. Reads the game's metadata: call after [prepare],
     * off the main thread.
     */
    fun gamePropertiesIntent(context: Context, rom: Rom): Intent? {
        val game = GameHelper.getGame(rom.uri, addedToLibrary = false) ?: return null
        return Intent(context, SettingsSubscreenActivity::class.java)
            .putExtras(SettingsSubscreenActivityArgs(SettingsSubscreen.GAME_PROPERTIES, game).toBundle())
    }

    /** One of the tools of Eden's home screen (profiles, GPU drivers, applets, data), opened directly. */
    fun toolIntent(context: Context, screen: SettingsSubscreen): Intent =
        Intent(context, SettingsSubscreenActivity::class.java).putExtras(SettingsSubscreenActivityArgs(screen, null).toBundle())

    /**
     * Sharing Eden's log ([gpu]: its GPU log), as its home screen's "Share debug logs" does: the log of the game
     * just played, or, when none was played since the app started, the previous run's (kept as .old.txt).
     * Null when there is no log.
     */
    fun shareLogIntent(context: Context, gpu: Boolean): Intent? {
        val name = if (gpu) "eden_gpu.log" else "eden_log.txt"
        fun log(file: String) = DocumentFile.fromSingleUri(
            context,
            DocumentsContract.buildDocumentUri(DocumentProvider.AUTHORITY, "${DocumentProvider.ROOT_ID}/log/$file"),
        )
        val current = log(name)
        val old = log("$name.old.txt")
        val shared = when {
            !EdenLog.gameLaunched && old?.exists() == true -> old
            current?.exists() == true -> current
            else -> return null
        }
        val send = Intent(Intent.ACTION_SEND)
            .setDataAndType(shared.uri, FileUtil.TEXT_PLAIN)
            .putExtra(Intent.EXTRA_STREAM, shared.uri)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        return Intent.createChooser(send, context.getString(if (gpu) org.yuzu.yuzu_emu.R.string.share_gpu_log else org.yuzu.yuzu_emu.R.string.eden_share_log))
    }
}
