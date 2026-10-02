package me.magnum.melonds.common.romprocessors

import android.graphics.Bitmap
import android.net.Uri
import me.magnum.melonds.common.uridelegates.UriHandler
import me.magnum.melonds.domain.model.RomInfo
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.config.RomConfig
import me.magnum.melonds.extensions.nameWithoutExtension

/**
 * 3DS games, run by the Azahar core. Only file information is read here: the title, icon and
 * other metadata need the 3DS core, which isn't loaded while browsing the game list.
 */
class N3dsRomFileProcessor(private val uriHandler: UriHandler) : RomFileProcessor {

    override fun getRomFromUri(romUri: Uri, parentUri: Uri?): Rom? {
        val romDocument = uriHandler.getUriDocument(romUri) ?: return null
        return Rom(
            name = romDocument.nameWithoutExtension ?: "",
            developerName = "",
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
}
