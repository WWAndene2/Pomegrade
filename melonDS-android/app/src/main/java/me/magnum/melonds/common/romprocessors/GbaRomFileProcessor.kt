package me.magnum.melonds.common.romprocessors

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import me.magnum.melonds.common.uridelegates.UriHandler
import me.magnum.melonds.domain.model.RomInfo
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.config.RomConfig
import me.magnum.melonds.extensions.nameWithoutExtension

/**
 * GBA games, run by the mGBA core. A GBA cartridge's header holds a 12-character title at 0xA0 and
 * a 2-character maker code at 0xB0; there is no icon. Games whose header has no title are listed
 * by their file name.
 */
class GbaRomFileProcessor(private val context: Context, private val uriHandler: UriHandler) : RomFileProcessor {

    companion object {
        private const val TITLE_OFFSET = 0xA0
        private const val TITLE_LENGTH = 12
        private const val MAKER_OFFSET = 0xB0
        private const val HEADER_LENGTH = 0xC0
    }

    override fun getRomFromUri(romUri: Uri, parentUri: Uri?): Rom? {
        val romDocument = uriHandler.getUriDocument(romUri) ?: return null
        val header = readHeader(romUri)
        val title = header?.let { RomHeaderText.ascii(it, TITLE_OFFSET, TITLE_LENGTH) }.orEmpty()
        val maker = header?.let { RomHeaderText.ascii(it, MAKER_OFFSET, 2) }.orEmpty()
        return Rom(
            name = title.ifEmpty { romDocument.nameWithoutExtension ?: "" },
            developerName = maker,
            fileName = romDocument.name ?: "",
            uri = romUri,
            parentTreeUri = parentUri,
            config = RomConfig.default(),
            lastPlayed = null,
            isDsiWareTitle = false,
            retroAchievementsHash = "",
        )
    }

    override fun getRomIcon(rom: Rom): Bitmap? = null

    override fun getRomInfo(rom: Rom): RomInfo? = null

    override suspend fun getRealRomUri(rom: Rom): Uri = rom.uri

    private fun readHeader(uri: Uri): ByteArray? = try {
        context.contentResolver.openInputStream(uri)?.use { input ->
            val header = ByteArray(HEADER_LENGTH)
            var read = 0
            while (read < HEADER_LENGTH) {
                val n = input.read(header, read, HEADER_LENGTH - read)
                if (n < 0) break
                read += n
            }
            header.takeIf { read == HEADER_LENGTH }
        }
    } catch (e: Exception) {
        null
    }
}
