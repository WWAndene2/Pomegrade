package me.magnum.melonds.ui.common.rom

import android.content.Context
import android.content.Intent
import androidx.preference.PreferenceManager
import io.github.wwandene2.pomegrade.gba.GbaEmulatorActivity
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.impl.PomegradeFolder
import me.magnum.melonds.impl.PunicaLayout
import java.io.File

/**
 * Starts GBA games in the mGBA core. The core reads the game, and writes its save and save states in the
 * folder's GBA/Saves and GBA/States, by file path, so it needs the folder's file access. A BIOS dumped from the
 * owner's console, `gba_bios.bin` in the Pomegrade folder's GBA/System, is used when present and the
 * setting allows it; mGBA's own otherwise. Its settings (Settings, GBA) are passed to the GBA screen here.
 */
object GbaLauncher : ConsoleLauncher {

    override fun isReady(context: Context): Boolean = PomegradeFolder.hasFileAccess(context)

    override val launchFailedMessage get() = R.string.gba_not_a_file

    /** The intent that runs [rom] in the GBA core, or null when the game isn't a file of the phone's storage. */
    override fun launchIntent(context: Context, rom: Rom): Intent? {
        val documentId = PomegradeFolder.documentIdOf(context, rom.uri) ?: return null
        val path = PomegradeFolder.pathOf(documentId)
        if (!path.isFile) return null
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        // GBA/Saves and GBA/States once the folder is in the layout of PunicaLayout; until it is moved, GBA/ itself,
        // where the earlier layout kept the saves and states (the migration sorts them from there)
        val (saveDir, statesDir) = if (PomegradeFolder.layoutRoot(context) != null) {
            PomegradeFolder.consoleFolder(context, PunicaLayout.GBA, PunicaLayout.SAVES) to PomegradeFolder.consoleFolder(context, PunicaLayout.GBA, PunicaLayout.STATES)
        } else {
            PomegradeFolder.subFolder(context, PunicaLayout.GBA) to null
        }
        listOfNotNull(saveDir, statesDir).forEach { it.mkdirs() }
        val bios = if (!preferences.getBoolean("gba_console_bios", true)) null else PomegradeFolder.consoleFolder(context, PunicaLayout.GBA, PunicaLayout.SYSTEM)?.let { File(it, PomegradeFolder.GBA_BIOS_FILE) }?.takeIf { it.isFile }
        return GbaEmulatorActivity.intent(
            context, path.absolutePath, rom.config.customName ?: rom.name, bios?.absolutePath,
            smoothScreen = preferences.getBoolean("gba_screen_smoothing", false),
            saveDir = saveDir?.absolutePath,
            statesDir = statesDir?.absolutePath,
        )
    }
}
