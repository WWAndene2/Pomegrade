package me.magnum.melonds.impl

import android.content.Context
import android.provider.DocumentsContract
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.domain.repositories.SettingsRepository
import java.io.File
import java.util.zip.ZipFile

/**
 * Checks the consoles' system files and puts them where Punica reads them, each console's System (PunicaLayout;
 * owner's request, 14 October): never deletes, moves. Runs when the game list opens and from the settings ("Check
 * the system files"):
 * - in Switch/System/Firmware, a firmware's .zip has its NCAs extracted next to it (the zip kept), so
 *   [SwitchSystemFiles] installs them in the Switch core's NAND at the next Switch launch;
 * - in the ROM folders, system files left among the games move into their console's System: prod.keys and
 *   title.keys into Switch/System, gba_bios.bin into GBA/System, a DS or DSi BIOS set (told apart by bios7.bin's
 *   size) into DS/System or DS/System/DSi, a whole firmware's NCAs (50 or more named by their hash in one folder)
 *   into Switch/System/Firmware when no firmware is there yet. A file whose name is already taken there stays
 *   where it is, and is reported.
 * A firmware the earlier folder organizer had scattered in Roms/<hash>/ is gathered by PunicaLayout.migrate.
 * Punica's folders that hold no game are never searched: the Switch core's NAND (Switch/System/nand) is where an
 * installed firmware belongs. Files by path (java.io), as [PomegradeFolder]. Background thread.
 */
object SystemFilesCheck {

    data class Report(val moved: List<String>, val left: List<String>, val extracted: Int)

    private val SWITCH_KEYS = listOf("prod.keys", "title.keys")
    // a firmware has a few hundred NCAs; an extracted game a handful
    private const val FIRMWARE_MIN_NCAS = 50
    private const val MAX_DEPTH = 8

    fun run(context: Context, settingsRepository: SettingsRepository): Report {
        val folder = settingsRepository.getPomegradeFolder() ?: return Report(emptyList(), emptyList(), 0)
        if (!PomegradeFolder.hasFileAccess(context)) return Report(emptyList(), emptyList(), 0)
        val root = PomegradeFolder.pathOf(DocumentsContract.getTreeDocumentId(folder))
        val switchBios = PunicaLayout.part(root, PunicaLayout.SWITCH, PunicaLayout.SYSTEM)
        val gbaBios = PunicaLayout.part(root, PunicaLayout.GBA, PunicaLayout.SYSTEM)
        val dsBios = PunicaLayout.part(root, PunicaLayout.DS, PunicaLayout.SYSTEM)
        val firmware = File(switchBios, PunicaLayout.FIRMWARE).apply { mkdirs() }
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
        } + PunicaLayout.CONSOLES.map { PunicaLayout.part(root, it, PunicaLayout.GAMES) }
        val visited = mutableSetOf<String>()
        fun search(dir: File, depth: Int) {
            // an unreadable folder is skipped, not the rest of the check
            val canonical = runCatching { dir.canonicalPath }.getOrNull() ?: return
            if (depth > MAX_DEPTH || dir.absolutePath in skip || !visited.add(canonical)) return
            val files = dir.listFiles() ?: return
            files.filter { it.isDirectory }.forEach { search(it, depth + 1) }
            val plain = files.filter { it.isFile }
            plain.filter { it.name.lowercase() in SWITCH_KEYS }.forEach { move(it, switchBios) }
            plain.filter { it.name.equals(PomegradeFolder.GBA_BIOS_FILE, true) }.forEach { move(it, gbaBios) }
            plain.firstOrNull { it.name.equals("bios7.bin", true) }?.let { bios7 ->
                val (target, names) = when (bios7.length()) {
                    PomegradeFolder.DS_BIOS7_SIZE -> dsBios to PomegradeFolder.DS_BIOS_FILES
                    PomegradeFolder.DSI_BIOS7_SIZE -> File(dsBios, PunicaLayout.DSI) to PomegradeFolder.DSI_BIOS_FILES
                    else -> return@let
                }
                plain.filter { f -> names.any { it.equals(f.name, true) } }.forEach { move(it, target) }
            }
            val ncas = plain.filter { RomPlatform.isSwitchContentFile(it.name) }
            if (ncas.size >= FIRMWARE_MIN_NCAS) {
                val hasFirmware = firmware.listFiles()?.any { it.name.endsWith(".nca", true) || it.name.endsWith(".zip", true) } == true
                if (hasFirmware) left += "${dir.path} (${ncas.size} NCA: ${firmware.path} has a firmware already)" else ncas.forEach { move(it, firmware) }
            }
        }
        searched.forEach { search(it, 0) }

        // a firmware's zip in Switch/System/Firmware: its NCAs extracted next to it, the zip kept
        var extracted = 0
        firmware.listFiles { f -> f.isFile && f.name.endsWith(".zip", true) }?.forEach { zip ->
            runCatching {
                ZipFile(zip).use { archive ->
                    archive.entries().asSequence().filter { !it.isDirectory && it.name.endsWith(".nca", true) }.forEach { entry ->
                        val target = File(firmware, entry.name.substringAfterLast('/'))
                        if (!target.exists()) {
                            // written aside, then renamed: a cut extraction is not taken for a whole file next time.
                            // One entry failing (storage full, a damaged entry) leaves no part behind and the others go on
                            val part = File(firmware, "${target.name}.part")
                            val done = runCatching {
                                archive.getInputStream(entry).use { input -> part.outputStream().use { input.copyTo(it) } }
                                part.renameTo(target)
                            }.getOrDefault(false)
                            if (done) extracted++ else { part.delete(); left += "${zip.path}: ${entry.name}" }
                        }
                    }
                }
            }.onFailure { left += zip.path }
        }
        return Report(moved, left, extracted)
    }
}
