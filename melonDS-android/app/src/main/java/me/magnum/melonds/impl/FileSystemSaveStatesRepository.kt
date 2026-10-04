package me.magnum.melonds.impl

import android.graphics.Bitmap
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import me.magnum.melonds.common.uridelegates.UriHandler
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.SaveStateSlot
import me.magnum.melonds.domain.repositories.SaveStatesRepository
import me.magnum.melonds.domain.repositories.SettingsRepository
import me.magnum.melonds.extensions.nameWithoutExtension
import me.magnum.melonds.ui.emulator.exceptions.SaveSlotLoadException
import java.util.*

class FileSystemSaveStatesRepository(
    private val settingsRepository: SettingsRepository,
    private val saveStateScreenshotProvider: SaveStateScreenshotProvider,
    private val uriHandler: UriHandler
) : SaveStatesRepository {

    companion object {
        const val SAVE_STATE_SLOT_COUNT = 11
    }

    override fun getRomSaveStates(rom: Rom): List<SaveStateSlot> {
        val saveStateDirectoryDocument = getSaveStateDirectoryDocument(rom) ?: return emptyList()
        val romFileName = getRomFileNameWithoutExtension(rom) ?: return emptyList()

        // Pomegrade: the quick slot (.ml0) and slots 1 to 10, as the 3DS core has
        val saveStateSlots = Array(SAVE_STATE_SLOT_COUNT) {
            SaveStateSlot(it, false, null, null)
        }
        val fileNameRegex = "${Regex.escape(romFileName)}\\.ml([0-9]|10)".toRegex()
        saveStateDirectoryDocument.listFiles().forEach {
            val fileName = it.name
            if (fileName?.matches(fileNameRegex) == true) {
                val slotNumber = fileName.substringAfterLast(".ml").toInt()
                val slot = SaveStateSlot(slotNumber, true, Date(it.lastModified()), null)
                val screenshotUri = saveStateScreenshotProvider.getRomSaveStateScreenshotUri(rom, slot)
                saveStateSlots[slotNumber] = slot.copy(screenshot = screenshotUri)
            }
        }

        return saveStateSlots.toList()
    }

    override fun getRomQuickSaveStateSlot(rom: Rom): SaveStateSlot {
        val quickSaveStateDocument = getRomQuickSaveStateDocument(rom)
        val saveStateExists = quickSaveStateDocument != null
        val lastModified = quickSaveStateDocument?.let { Date(it.lastModified()) }
        val slot = SaveStateSlot(SaveStateSlot.QUICK_SAVE_SLOT, saveStateExists, lastModified, null)
        val screenshotUri = saveStateScreenshotProvider.getRomSaveStateScreenshotUri(rom, slot)
        return slot.copy(screenshot = screenshotUri)
    }

    override fun getRomSaveStateUri(rom: Rom, saveState: SaveStateSlot): Uri {
        val saveStateDirectoryDocument = getSaveStateDirectoryDocument(rom) ?: throw SaveSlotLoadException("Could not create parent directory document")

        val romFileName = getRomFileNameWithoutExtension(rom) ?: throw SaveSlotLoadException("Could not determine ROM file name")
        val saveStateName = "$romFileName.ml${saveState.slot}"
        val saveStateFile = saveStateDirectoryDocument.findFile(saveStateName)

        val uri = if (saveStateFile != null) {
            saveStateFile.uri
        } else {
            saveStateDirectoryDocument.createFile("*/*", saveStateName)?.uri ?: throw SaveSlotLoadException("Could not create save state file")
        }

        return uri
    }

    override fun setRomSaveStateScreenshot(rom: Rom, saveState: SaveStateSlot, screenshot: Bitmap) {
        saveStateScreenshotProvider.saveRomSaveStateScreenshot(rom, saveState, screenshot)
    }

    override fun deleteRomSaveStateScreenshot(rom: Rom, saveState: SaveStateSlot) {
        saveStateScreenshotProvider.deleteRomSaveStateScreenshot(rom, saveState)
    }

    override fun deleteRomSaveState(rom: Rom, saveState: SaveStateSlot) {
        if (!saveState.exists) {
            return
        }

        val saveStateDirectoryDocument = getSaveStateDirectoryDocument(rom) ?: throw SaveSlotLoadException("Could not create parent directory document")
        val romFileName = getRomFileNameWithoutExtension(rom) ?: throw SaveSlotLoadException("Could not determine ROM file name")

        val saveStateName = "$romFileName.ml${saveState.slot}"
        val saveStateFile = saveStateDirectoryDocument.findFile(saveStateName)

        saveStateFile?.delete()
        saveStateScreenshotProvider.deleteRomSaveStateScreenshot(rom, saveState)
    }

    private fun getRomQuickSaveStateDocument(rom: Rom): DocumentFile? {
        val saveStateDirectoryDocument = getSaveStateDirectoryDocument(rom) ?: return null
        val romFileName = getRomFileNameWithoutExtension(rom) ?: return null

        val quickSaveStateFileName = "$romFileName.ml0"
        return saveStateDirectoryDocument.findFile(quickSaveStateFileName)
    }

    private fun getSaveStateDirectoryDocument(rom: Rom): DocumentFile? {
        val saveStateDirectoryUri = settingsRepository.getSaveStateDirectory(rom) ?: return null
        return uriHandler.getUriTreeDocument(saveStateDirectoryUri)
    }

    private fun getRomFileNameWithoutExtension(rom: Rom): String? {
        val romDocument = uriHandler.getUriDocument(rom.uri)
        return romDocument?.nameWithoutExtension
    }
}