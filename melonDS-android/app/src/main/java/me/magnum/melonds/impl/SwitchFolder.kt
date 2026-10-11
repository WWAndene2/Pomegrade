package me.magnum.melonds.impl

import android.content.Context
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.io.File

/**
 * The Switch core's data in the Pomegrade folder's Switch, as the 3DS core's is in its 3DS:
 *
 * ```
 * Pomegrade/Switch/
 * ├── nand/   the Switch's internal memory: game saves, installed updates and DLC, the firmware
 * └── sdmc/   its SD card: mods, screenshots
 * ```
 *
 * Eden keeps them in the app's own folder (Android/data), which Android deletes with the app; here they
 * stay, with the other consoles' saves. The first time, what Eden already has there is moved; if
 * Pomegrade/Switch already has data (after a reinstall), that data is used and Eden's is left. Folders
 * the owner picked in Eden's own settings are left alone. Needs Eden started (YuzuApplication.start, on
 * the main thread); runs on a background thread.
 */
object SwitchFolder {

    private const val NAND = "nand"
    private const val SDMC = "sdmc"

    fun setUp(context: Context) {
        val root = PomegradeFolder.subFolder(context, PomegradeFolder.SWITCH) ?: return
        val userDirectory = File(DirectoryInitialization.userDirectory ?: return)
        val nand = File(root, NAND)
        val sdmc = File(root, SDMC)
        val currentNand = File(NativeConfig.getNandDir()).canonicalFile
        if (currentNand == nand.canonicalFile) return
        if (currentNand != File(userDirectory, NAND).canonicalFile) return // the owner's own choice in Eden

        moveIfAbsent(File(userDirectory, NAND), nand)
        if (File(NativeConfig.getSdmcDir()).canonicalFile == File(userDirectory, SDMC).canonicalFile) {
            moveIfAbsent(File(userDirectory, SDMC), sdmc)
            NativeConfig.setSdmcDir(sdmc.absolutePath)
        }
        // the saves folder follows the NAND unless set apart (Eden's default is the NAND itself)
        if (File(NativeConfig.getSaveDir()).canonicalFile == currentNand) NativeConfig.setSaveDir(nand.absolutePath)
        NativeConfig.setNandDir(nand.absolutePath)
        NativeConfig.saveGlobalConfig()
        NativeLibrary.initializeSystem(true)
    }

    private fun moveIfAbsent(source: File, target: File) {
        if (target.exists()) {
            target.mkdirs()
            return
        }
        target.parentFile?.mkdirs()
        if (!source.exists()) {
            target.mkdirs()
        } else if (!source.renameTo(target)) {
            source.copyRecursively(target)
            source.deleteRecursively()
        }
    }
}
