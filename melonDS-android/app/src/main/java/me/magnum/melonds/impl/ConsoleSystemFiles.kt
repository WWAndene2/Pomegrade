package me.magnum.melonds.impl

import java.io.File

/**
 * The owner's console system files, kept in the folder's BIOS with every console's (owner's decision,
 * 13 October), copied where each core reads them, as SwitchSystemFiles does for the Switch's keys and
 * firmware:
 *
 * ```
 * BIOS/3DS/              aes_keys.txt, boot9.bin, seeddb.bin... -> 3DS/sysdata/ (Azahar's SYSDATA_DIR)
 * BIOS/GC-Wii/<region>/  IPL.bin, region USA, EUR or JAP        -> GC-Wii/GC/<region>/IPL.bin (Dolphin's boot ROM)
 * ```
 *
 * Copied only when missing or changed, so it is cheap at every game list opening; a file removed from BIOS
 * stays in the core's folder. Files only (java.io). Background thread.
 */
object ConsoleSystemFiles {

    val GC_REGIONS = listOf("USA", "EUR", "JAP")
    private const val GC_IPL = "IPL.bin"

    /** [bios] is the folder's BIOS, [root] the folder itself. */
    fun import(bios: File, root: File) {
        File(bios, PomegradeFolder.N3DS).listFiles()?.filter { it.isFile }?.forEach { file ->
            copyIfChanged(file, File(File(root, "${PomegradeFolder.N3DS}/sysdata"), file.name))
        }
        GC_REGIONS.forEach { region ->
            val ipl = File(File(File(bios, PomegradeFolder.GC_WII), region), GC_IPL)
            if (ipl.isFile) copyIfChanged(ipl, File(File(root, "${PomegradeFolder.GC_WII}/GC/$region"), GC_IPL))
        }
    }

    private fun copyIfChanged(from: File, to: File) {
        if (to.isFile && to.length() == from.length() && to.readBytes().contentEquals(from.readBytes())) return
        to.parentFile?.mkdirs()
        from.copyTo(to, overwrite = true)
    }
}
