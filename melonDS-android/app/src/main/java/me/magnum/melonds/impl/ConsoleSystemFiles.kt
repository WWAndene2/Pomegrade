package me.magnum.melonds.impl

import java.io.File

/**
 * The owner's console system files, kept in each console's System (PunicaLayout), copied where a core reads them
 * from its own folder, as SwitchSystemFiles does for the Switch's keys and firmware:
 *
 * ```
 * GC-Wii/System/<region>/IPL.bin, region USA, EUR or JAP -> GC-Wii/Core/GC/<region>/IPL.bin (Dolphin's boot ROM)
 * ```
 *
 * The 3DS keys need no copy: Azahar reads them in 3DS/System itself. Copied only when missing or changed, so it
 * is cheap at every game list opening; a file removed from System stays in the core's folder. Files only
 * (java.io). Background thread.
 */
object ConsoleSystemFiles {

    val GC_REGIONS = listOf("USA", "EUR", "JAP")
    private const val GC_IPL = "IPL.bin"

    /** [root] is the Pomegrade folder. */
    fun import(root: File) {
        GC_REGIONS.forEach { region ->
            val ipl = File(File(PunicaLayout.part(root, PunicaLayout.GC_WII, PunicaLayout.SYSTEM), region), GC_IPL)
            if (ipl.isFile) copyIfChanged(ipl, File(File(PunicaLayout.part(root, PunicaLayout.GC_WII, PunicaLayout.CORE), "GC/$region"), GC_IPL))
        }
    }

    private fun copyIfChanged(from: File, to: File) {
        if (to.isFile && to.length() == from.length() && to.readBytes().contentEquals(from.readBytes())) return
        to.parentFile?.mkdirs()
        from.copyTo(to, overwrite = true)
    }
}
