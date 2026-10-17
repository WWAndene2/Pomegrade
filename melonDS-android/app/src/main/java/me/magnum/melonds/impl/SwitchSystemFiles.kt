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
 * Punica/Switch/System/
 * ├── prod.keys, title.keys   copied into Eden's keys folder (Switch/Core/keys), as Eden's "Install keys" does
 * └── Firmware/ (its .nca)    copied into Eden's NAND, as Eden's "Install firmware" does with a zip's NCAs
 * ```
 *
 * Copied only when the installed ones differ (the keys' bytes, the NCAs' names and sizes in the NAND Eden uses now),
 * so it is cheap on every launch. Eden's own setup screen still works too;
 * a file found here replaces the one installed there. Needs Eden started (YuzuApplication.start); runs on a
 * background thread: a firmware is ~300 MB.
 */
object SwitchSystemFiles {

    private const val FIRMWARE = "Firmware"
    private val KEY_FILES = listOf("prod.keys", "title.keys")
    // the firmware last copied, as it was told before: its NCAs' names and sizes in the app's preferences, and
    // earlier a file in Eden's user folder. Neither named the NAND that received them, so a new NAND (another
    // Punica folder) got no firmware: the NAND itself is now checked, these are removed when found
    private const val OLD_FIRMWARE_SIGNATURE = "switch_firmware_signature"
    private const val OLD_FIRMWARE_MARKER = "pomegrade_firmware.txt"

    fun import(context: Context) {
        val source = PomegradeFolder.consoleFolder(context, PunicaLayout.SWITCH, PunicaLayout.SYSTEM)?.takeIf { it.isDirectory } ?: return
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

    /** Copies the firmware's NCAs into Eden's current NAND when it does not hold them all (same names and sizes). */
    private fun importFirmware(context: Context, source: File, userDirectory: File): Boolean {
        File(userDirectory, OLD_FIRMWARE_MARKER).delete()
        val preferences = PreferenceManager.getDefaultSharedPreferences(context)
        if (preferences.contains(OLD_FIRMWARE_SIGNATURE)) preferences.edit().remove(OLD_FIRMWARE_SIGNATURE).apply()
        val ncas = source.listFiles { file -> file.isFile && file.name.endsWith(".nca") }?.sortedBy { it.name }.orEmpty()
        if (ncas.isEmpty()) return false
        val registered = File(NativeConfig.getNandDir(), "system/Contents/registered")
        if (holdsFirmware(registered, ncas)) return false

        registered.deleteRecursively()
        registered.mkdirs()
        ncas.forEach { it.copyTo(File(registered, it.name), overwrite = true) }
        return true
    }

    /** Whether [registered] (a NAND's system/Contents/registered) holds each of [ncas], told by name and size. */
    internal fun holdsFirmware(registered: File, ncas: List<File>): Boolean =
        ncas.all { nca -> File(registered, nca.name).let { it.isFile && it.length() == nca.length() } }
}
