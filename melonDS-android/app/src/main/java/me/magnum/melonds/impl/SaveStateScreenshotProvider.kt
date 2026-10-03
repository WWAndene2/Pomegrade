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
    fun relocateRomScreenshots(rom: Rom, newUri: Uri) {
        val from = File(getScreenshotsDir(), rom.uri.hashCode().toString())
        val to = File(getScreenshotsDir(), newUri.hashCode().toString())
        if (from.isDirectory && !to.exists()) {
            from.renameTo(to)
        }
    }

    private fun getRomSaveStateScreenshotFile(rom: Rom, saveState: SaveStateSlot, createDirectoriesIfNeeded: Boolean = false): File? {
        val romDirectoryName = rom.uri.hashCode().toString()
        val romDirectory = File(getScreenshotsDir(), romDirectoryName)

        if (!romDirectory.isDirectory && createDirectoriesIfNeeded && !romDirectory.mkdirs()) {
            return null
        }

        return File(romDirectory, "${saveState.slot}.png")
    }

    private fun getScreenshotsDir(): File {
        // Pomegrade: in the Pomegrade folder, which stays when the app is uninstalled
        return PomegradeFolder.subFolder(context, PomegradeFolder.SAVE_STATE_PREVIEWS)?.takeIf { it.isDirectory || it.mkdirs() }
            ?: File(context.filesDir, SAVE_STATE_SCREENSHOTS_DIR)
    }

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