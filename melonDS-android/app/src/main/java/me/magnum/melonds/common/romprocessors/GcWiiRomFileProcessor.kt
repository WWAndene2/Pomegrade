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
 * GameCube and Wii games, run by the Dolphin core. A disc's header starts with its 6-character game ID
 * (the maker code is its last 2) and has the game's title at 0x20. The header is read where it is
 * stored uncompressed: at the start of a plain image (iso, gcm), at 0x58 in WIA and RVZ files (inside
 * their second header) and at 0x8000 in CISO files. Other files (gcz, wbfs, wad, homebrew) are listed by
 * their file name; Dolphin's own game list shows their real titles and covers.
 */
class GcWiiRomFileProcessor(private val context: Context, private val uriHandler: UriHandler) : RomFileProcessor {

    companion object {
        private const val TITLE_OFFSET = 0x20
        private const val TITLE_LENGTH = 0x40
        private const val MAKER_OFFSET = 4
        private const val HEADER_LENGTH = TITLE_OFFSET + TITLE_LENGTH
        private val HEADER_POSITION = mapOf("iso" to 0L, "gcm" to 0L, "wia" to 0x58L, "rvz" to 0x58L, "ciso" to 0x8000L)
    }

    override fun getRomFromUri(romUri: Uri, parentUri: Uri?): Rom? {
        val romDocument = uriHandler.getUriDocument(romUri) ?: return null
        val fileName = romDocument.name ?: ""
        val header = HEADER_POSITION[fileName.substringAfterLast('.', "").lowercase()]?.let { readHeader(romUri, it) }
        val title = header?.let { ascii(it, TITLE_OFFSET, TITLE_LENGTH) }.orEmpty()
        val maker = header?.let { ascii(it, MAKER_OFFSET, 2) }.orEmpty()
        return Rom(
            name = title.ifEmpty { romDocument.nameWithoutExtension ?: "" },
            developerName = maker,
            fileName = fileName,
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

    private fun readHeader(uri: Uri, position: Long): ByteArray? = try {
        context.contentResolver.openInputStream(uri)?.use { input ->
            var skipped = 0L
            while (skipped < position) {
                val n = input.skip(position - skipped)
                if (n <= 0) break
                skipped += n
            }
            val header = ByteArray(HEADER_LENGTH)
            var read = 0
            while (skipped == position && read < HEADER_LENGTH) {
                val n = input.read(header, read, HEADER_LENGTH - read)
                if (n < 0) break
                read += n
            }
            header.takeIf { read == HEADER_LENGTH }
        }
    } catch (e: Exception) {
        null
    }

    // printable ASCII up to the first NUL, trimmed (titles of Japanese discs are Shift JIS: dropped)
    private fun ascii(bytes: ByteArray, offset: Int, length: Int): String =
        bytes.copyOfRange(offset, offset + length)
            .takeWhile { it != 0.toByte() }
            .filter { it in 0x20..0x7E }
            .toByteArray()
            .toString(Charsets.US_ASCII)
            .trim()
}
