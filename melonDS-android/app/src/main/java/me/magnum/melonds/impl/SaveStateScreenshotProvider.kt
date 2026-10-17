package me.magnum.melonds.impl

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import com.squareup.picasso.Picasso
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.SaveStateSlot
import java.io.File

class SaveStateScreenshotProvider(
    private val context: Context,
    private val picasso: Picasso
) {

    companion object {
        private const val SAVE_STATE_SCREENSHOTS_DIR = "ss_screenshots"

        /** The screenshots of the game whose file was at [oldUri], now at [newUri] (kept under its URI's hash). */
        fun relocateRomScreenshots(context: Context, oldUri: Uri, newUri: Uri) {
            val dir = getScreenshotsDir(context)
            val from = File(dir, oldUri.hashCode().toString())
            val to = File(dir, newUri.hashCode().toString())
            if (from.isDirectory && !to.exists()) {
                from.renameTo(to)
            }
        }

        private fun getScreenshotsDir(context: Context): File {
            // Pomegrade: in the Pomegrade folder, which stays when the app is uninstalled: DS/States/Previews once it
            // is in the layout of PunicaLayout, the earlier "Save state previews" until it is moved
            val folder = if (PomegradeFolder.layoutRoot(context) != null) {
                PomegradeFolder.consoleFolder(context, PunicaLayout.DS, "${PunicaLayout.STATES}/${PunicaLayout.PREVIEWS}")
            } else {
                PomegradeFolder.subFolder(context, "Save state previews")
            }
            return folder?.takeIf { it.isDirectory || it.mkdirs() } ?: File(context.filesDir, SAVE_STATE_SCREENSHOTS_DIR)
        }
    }

    fun saveRomSaveStateScreenshot(rom: Rom, saveState: SaveStateSlot, screenshot: Bitmap) {
        val screenshotFile = getRomSaveStateScreenshotFile(rom, saveState, true) ?: return
        screenshotFile.outputStream().use {
            screenshot.compress(Bitmap.CompressFormat.PNG, 100, it)
        }

        invalidateScreenshotFile(screenshotFile)
    }

    fun getRomSaveStateScreenshotUri(rom: Rom, saveState: SaveStateSlot): Uri? {
        val screenshotFile = getRomSaveStateScreenshotFile(rom, saveState) ?: return null
        return if (screenshotFile.isFile) {
            DocumentFile.fromFile(screenshotFile).uri
        } else {
            null
        }
    }

    fun deleteRomSaveStateScreenshot(rom: Rom, saveState: SaveStateSlot) {
        getRomSaveStateScreenshotFile(rom, saveState)?.let {
            invalidateScreenshotFile(it)
            it.delete()
        }
    }

    // the ROM's file moved to newUri: its screenshots are kept under its address's hash
    fun relocateRomScreenshots(rom: Rom, newUri: Uri) = relocateRomScreenshots(rom.uri, newUri)

    /** The screenshots of the game whose file was at [oldUri], now at [newUri]. */
    fun relocateRomScreenshots(oldUri: Uri, newUri: Uri) = relocateRomScreenshots(context, oldUri, newUri)

    private fun getRomSaveStateScreenshotFile(rom: Rom, saveState: SaveStateSlot, createDirectoriesIfNeeded: Boolean = false): File? {
        val romDirectoryName = rom.uri.hashCode().toString()
        val romDirectory = File(getScreenshotsDir(), romDirectoryName)

        if (!romDirectory.isDirectory && createDirectoriesIfNeeded && !romDirectory.mkdirs()) {
            return null
        }

        return File(romDirectory, "${saveState.slot}.png")
    }

    private fun getScreenshotsDir(): File = getScreenshotsDir(context)

    /** Moves the screenshots kept in the app's own folder to [directory] (the Pomegrade folder's). */
    fun moveTo(directory: File) {
        val current = File(context.filesDir, SAVE_STATE_SCREENSHOTS_DIR)
        if (!current.isDirectory || !(directory.isDirectory || directory.mkdirs())) return
        current.listFiles()?.forEach { rom ->
            val target = File(directory, rom.name)
            if (!target.exists()) rom.renameTo(target) || rom.copyRecursively(target) && rom.deleteRecursively()
        }
    }

    private fun invalidateScreenshotFile(screenshotFile: File) {
        val screenshotDocument = DocumentFile.fromFile(screenshotFile)
        picasso.invalidate(screenshotDocument.uri)
    }
}