package me.magnum.melonds.common.romprocessors

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import me.magnum.melonds.common.uridelegates.UriHandler
import me.magnum.melonds.domain.model.RomInfo
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.config.RomConfig
import me.magnum.melonds.impl.SwitchGameMetadata

/**
 * Switch games, run by the Eden core. Their title and icon are encrypted: read with the owner's keys by the Switch
 * core ([SwitchGameMetadata], which starts it the first time); without keys, the title is the file's name without
 * its dump tags, and there is no icon.
 */
class SwitchRomFileProcessor(private val context: Context, private val uriHandler: UriHandler) : RomFileProcessor {

    override fun getRomFromUri(romUri: Uri, parentUri: Uri?): Rom? {
        val romDocument = uriHandler.getUriDocument(romUri) ?: return null
        val fileName = romDocument.name ?: ""
        return Rom(
            name = SwitchGameMetadata.title(context, romUri) ?: SwitchGameMetadata.cleanFileName(fileName),
            developerName = "",
            fileName = fileName,
            uri = romUri,
            parentTreeUri = parentUri,
            config = RomConfig.default(),
            lastPlayed = null,
            isDsiWareTitle = false,
            retroAchievementsHash = "",
        )
    }

    override fun getRomIcon(rom: Rom): Bitmap? = SwitchGameMetadata.icon(context, rom.uri)

    override fun getRomInfo(rom: Rom): RomInfo? = null

    override suspend fun getRealRomUri(rom: Rom): Uri = rom.uri
}
