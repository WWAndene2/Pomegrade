package me.magnum.melonds.impl

import android.content.Context
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.io.File

/**
 * Gives the Switch core the owner's keys and firmware from the Pomegrade folder, so they are set up in one
 * place with the other consoles' BIOS:
 *
 * ```
 * Pomegrade/BIOS/Switch/
 * ├── prod.keys, title.keys   copied into Eden's keys folder, as Eden's "Install keys" does
 * └── Firmware/ (its .nca)    copied into Eden's NAND, as Eden's "Install firmware" does with a zip's NCAs
 * ```
 *
 * Copied only when they changed, so it is cheap on every launch. Eden's own setup screen still works too;
 * a file found here replaces the one installed there. Needs Eden started (YuzuApplication.start); runs on a
 * background thread: a firmware is ~300 MB.
 */
object SwitchSystemFiles {

    private const val FIRMWARE = "Firmware"
    private val KEY_FILES = listOf("prod.keys", "title.keys")
    // the firmware last copied (its NCAs' names and sizes), kept in the app's preferences. It was a file in
    // Eden's user folder, which is now Punica/Switch, where it had no place: removed when found
    private const val FIRMWARE_SIGNATURE = "switch_firmware_signature"
    private const val OLD_FIRMWARE_MARKER = "pomegrade_firmware.txt"

    fun import(context: Context) {
        val source = PomegradeFolder.subFolder(context, "${PomegradeFolder.BIOS}/${PomegradeFolder.BIOS_SWITCH}")?.takeIf { it.isDirectory } ?: return
        val userDirectory = File(DirectoryInitialization.userDirectory ?: return)

        val keysChanged = KEY_FILES.map { File(source, it) }.filter { it.isFile }.count { key ->
            val installed = File(userDirectory, "keys/${key.name}")
            val changed = !installed.isFile || !installed.readBytes().contentEquals(key.readBytes())
            if (changed) key.copyTo(installed, overwrite = true)
            changed
        } > 0

        val firmwareChanged = importFirmware(context, File(source, FIRMWARE), userDirectory)

        if (firmwareChanged) {
            NativeLibrary.initializeSystem(true) // reloads the keys too
        } else if (keysChanged) {
            NativeLibrary.reloadKeys()
        }
    }

    /** Copies the firmware's NCAs when they differ from the last ones copied, told by their names and sizes. */
    private fun importFirmware(context: Context, source: File, userDirectory: File): Boolean {
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        val oldMarker = File(userDirectory, OLD_FIRMWARE_MARKER)
        if (oldMarker.isFile) {
            preferences.edit().putString(FIRMWARE_SIGNATURE, oldMarker.readText()).apply()
            oldMarker.delete()
        }
        val ncas = source.listFiles { file -> file.isFile && file.name.endsWith(".nca") }?.sortedBy { it.name }.orEmpty()
        if (ncas.isEmpty()) return false
        val signature = ncas.joinToString("\n") { "${it.name} ${it.length()}" }
        if (preferences.getString(FIRMWARE_SIGNATURE, null) == signature) return false

        val registered = File(NativeConfig.getNandDir(), "system/Contents/registered")
        registered.deleteRecursively()
        registered.mkdirs()
        ncas.forEach { it.copyTo(File(registered, it.name), overwrite = true) }
        preferences.edit().putString(FIRMWARE_SIGNATURE, signature).apply()
        return true
    }
}
