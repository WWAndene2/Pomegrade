package me.magnum.melonds.ui.common.rom

import android.content.Context
import android.content.Intent
import io.github.wwandene2.pomegrade.gba.GbaEmulatorActivity
import me.magnum.melonds.R
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.impl.PomegradeFolder
import java.io.File

/**
 * Starts GBA games in the mGBA core. The core reads the game, and writes its save and save states
 * next to it, by file path, so it needs the Pomegrade folder's file access. A BIOS dumped from the
 * owner's console, `gba_bios.bin` in the Pomegrade folder's BIOS/GBA, is used when present; mGBA's own
 * otherwise.
 */
object GbaLauncher : ConsoleLauncher {

    override fun isReady(context: Context): Boolean = PomegradeFolder.hasFileAccess(context)

    override val launchFailedMessage get() = R.string.gba_not_a_file

    // the GBA screen is not exported, so the launcher can't open it: no home-screen shortcut for GBA games yet
    override fun shortcutIntent(context: Context, rom: Rom): Intent? = null

    override val noShortcutMessage get() = R.string.gba_no_shortcut

    /** The intent that runs [rom] in the GBA core, or null when the game isn't a file of the phone's storage. */
    override fun launchIntent(context: Context, rom: Rom): Intent? {
        val documentId = PomegradeFolder.documentIdOf(context, rom.uri) ?: return null
        val path = PomegradeFolder.pathOf(documentId)
        if (!path.isFile) return null
        val bios = PomegradeFolder.subFolder(context, "${PomegradeFolder.BIOS}/${PomegradeFolder.BIOS_GBA}")?.let { File(it, PomegradeFolder.GBA_BIOS_FILE) }?.takeIf { it.isFile }
        return GbaEmulatorActivity.intent(context, path.absolutePath, rom.config.customName ?: rom.name, bios?.absolutePath)
    }
}
