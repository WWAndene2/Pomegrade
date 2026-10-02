package me.magnum.melonds.impl.romprocessors

import android.content.Context
import me.magnum.melonds.common.romprocessors.N3dsRomFileProcessor
import me.magnum.melonds.common.romprocessors.NdsRomFileProcessor
import me.magnum.melonds.common.romprocessors.RomFileProcessor
import me.magnum.melonds.common.romprocessors.SevenZRomFileProcessor
import me.magnum.melonds.common.romprocessors.ZipRomFileProcessor
import me.magnum.melonds.common.uridelegates.UriHandler
import me.magnum.melonds.domain.model.rom.RomPlatform
import me.magnum.melonds.impl.NdsRomCache

class Api24RomFileProcessorFactory(context: Context, uriHandler: UriHandler, ndsRomCache: NdsRomCache) : BaseRomFileProcessorFactory(context) {

    private val prefixProcessorMap: Map<String, RomFileProcessor>

    init {
        val ndsRomFileProcessor = NdsRomFileProcessor(context, uriHandler)
        val n3dsRomFileProcessor = N3dsRomFileProcessor(uriHandler)
        prefixProcessorMap = RomPlatform.n3dsExtensions.associateWith { n3dsRomFileProcessor } + mapOf(
            "nds" to ndsRomFileProcessor,
            "dsi" to ndsRomFileProcessor,
            "ids" to ndsRomFileProcessor,
            "zip" to ZipRomFileProcessor(context, uriHandler, ndsRomCache),
            "7z" to SevenZRomFileProcessor(context, uriHandler, ndsRomCache)
        )
    }

    override fun getRomFileProcessorForFileExtension(extension: String): RomFileProcessor? {
        return prefixProcessorMap[extension]
    }
}