package me.magnum.melonds.common.romprocessors

import android.graphics.Bitmap
import android.net.Uri
import me.magnum.melonds.common.uridelegates.UriHandler
import me.magnum.melonds.domain.model.RomInfo
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.config.RomConfig
import me.magnum.melonds.extensions.nameWithoutExtension

/**
 * Switch games, run by the Eden core. A Switch game's title and icon are encrypted: reading them needs the
 * owner's keys and the Switch core started, which listing the games shouldn't do. They are listed by their
 * file name, without an icon; Eden's own game list (Settings, Switch) shows the real ones.
 */
class SwitchRomFileProcessor(private val uriHandler: UriHandler) : RomFileProcessor {

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
