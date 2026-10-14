package me.magnum.melonds.impl

import android.content.Context
import android.provider.DocumentsContract
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.domain.repositories.SettingsRepository
import java.io.File
import java.util.zip.ZipFile

/**
 * Checks the consoles' system files and puts them where Punica reads them (owner's request, 14 October): never
 * deletes, moves. Runs when the game list opens and from the settings ("Check the system files"):
 * - in BIOS/Switch/Firmware, a firmware's .zip has its NCAs extracted next to it (the zip kept), so
 *   [SwitchSystemFiles] installs them in the Switch core's NAND at the next Switch launch;
 * - in the ROM folders, system files left among the games move into their BIOS folder: prod.keys and title.keys
 *   into BIOS/Switch, gba_bios.bin into BIOS/GBA, a DS or DSi BIOS set (told apart by bios7.bin's size) into BIOS/DS
 *   or BIOS/DSi, a whole firmware's NCAs (50 or more named by their hash in one folder) into BIOS/Switch/Firmware
 *   when no firmware is there yet. A file whose name is already taken there stays where it is, and is reported.
 * Punica's own console folders are never searched: the Switch core's NAND (Switch/nand) is where an installed
 * firmware belongs. Files by path (java.io), as [PomegradeFolder]. Background thread.
 */
object SystemFilesCheck {

    data class Report(val moved: List<String>, val left: List<String>, val extracted: Int)

    private val SWITCH_KEYS = listOf("prod.keys", "title.keys")
    private const val FIRMWARE = "Firmware"
    // a firmware has a few hundred NCAs; an extracted game a handful
    private const val FIRMWARE_MIN_NCAS = 50
    private const val MAX_DEPTH = 8

    fun run(context: Context, settingsRepository: SettingsRepository): Report {
        val folder = settingsRepository.getPomegradeFolder() ?: return Report(emptyList(), emptyList(), 0)
        if (!PomegradeFolder.hasFileAccess(context)) return Report(emptyList(), emptyList(), 0)
        val root = PomegradeFolder.pathOf(DocumentsContract.getTreeDocumentId(folder))
        val bios = File(root, PomegradeFolder.BIOS)
        val switchBios = File(bios, PomegradeFolder.BIOS_SWITCH)
        val firmware = File(switchBios, FIRMWARE).apply { mkdirs() }
        val moved = mutableListOf<String>()
        val left = mutableListOf<String>()

        fun move(file: File, into: File) {
            val target = File(into, file.name)
            if (target.exists()) { left += file.path; return }
            into.mkdirs()
            // renamed on the same storage; else copied aside (a cut copy is never taken for the file), renamed in once
            // whole, and only then the original removed. An original that cannot be removed counts as left (the copy
            // stays: both exist), never as moved
            if (file.renameTo(target)) { moved += "${file.path} -> ${target.path}"; return }
            val part = File(into, "${file.name}.part")
            val copied = runCatching { file.copyTo(part, overwrite = true); part.length() == file.length() && part.renameTo(target) }.getOrDefault(false)
            if (!copied) { part.delete(); left += file.path; return }
            if (file.delete()) moved += "${file.path} -> ${target.path}" else left += file.path
        }

        // the ROM folders, not Punica's console and system folders
        val skip = (PomegradeFolder.systemFolderDocumentIds(folder)).map { PomegradeFolder.pathOf(it).absolutePath }.toSet()
        val searched = settingsRepository.getRomSearchDirectories().mapNotNull { uri ->
            runCatching { PomegradeFolder.pathOf(DocumentsContract.getTreeDocumentId(uri)) }.getOrNull()
        } + File(root, PomegradeFolder.ROMS)
        val visited = mutableSetOf<String>()
        fun search(dir: File, depth: Int) {
            if (depth > MAX_DEPTH || dir.absolutePath in skip || !visited.add(dir.canonicalPath)) return
            val files = dir.listFiles() ?: return
            files.filter { it.isDirectory }.forEach { search(it, depth + 1) }
            val plain = files.filter { it.isFile }
            plain.filter { it.name.lowercase() in SWITCH_KEYS }.forEach { move(it, switchBios) }
            plain.filter { it.name.equals(PomegradeFolder.GBA_BIOS_FILE, true) }.forEach { move(it, File(bios, PomegradeFolder.BIOS_GBA)) }
            plain.firstOrNull { it.name.equals("bios7.bin", true) }?.let { bios7 ->
                val (console, names) = when (bios7.length()) {
                    PomegradeFolder.DS_BIOS7_SIZE -> PomegradeFolder.BIOS_DS to PomegradeFolder.DS_BIOS_FILES
                    PomegradeFolder.DSI_BIOS7_SIZE -> PomegradeFolder.BIOS_DSI to PomegradeFolder.DSI_BIOS_FILES
                    else -> return@let
                }
                plain.filter { f -> names.any { it.equals(f.name, true) } }.forEach { move(it, File(bios, console)) }
            }
            val ncas = plain.filter { RomPlatform.isSwitchContentFile(it.name) }
            if (ncas.size >= FIRMWARE_MIN_NCAS) {
                val hasFirmware = firmware.listFiles()?.any { it.name.endsWith(".nca", true) || it.name.endsWith(".zip", true) } == true
                if (hasFirmware) left += "${dir.path} (${ncas.size} NCA: ${firmware.path} has a firmware already)" else ncas.forEach { move(it, firmware) }
            }
        }
        searched.forEach { search(it, 0) }

        // a firmware's zip in BIOS/Switch/Firmware: its NCAs extracted next to it, the zip kept
        var extracted = 0
        firmware.listFiles { f -> f.isFile && f.name.endsWith(".zip", true) }?.forEach { zip ->
            runCatching {
                ZipFile(zip).use { archive ->
                    archive.entries().asSequence().filter { !it.isDirectory && it.name.endsWith(".nca", true) }.forEach { entry ->
                        val target = File(firmware, entry.name.substringAfterLast('/'))
                        if (!target.exists()) {
                            // written aside, then renamed: a cut extraction is not taken for a whole file next time
                            val part = File(firmware, "${target.name}.part")
                            archive.getInputStream(entry).use { input -> part.outputStream().use { input.copyTo(it) } }
                            if (part.renameTo(target)) extracted++ else part.delete()
                        }
                    }
                }
            }.onFailure { left += zip.path }
        }
        return Report(moved, left, extracted)
    }
}
