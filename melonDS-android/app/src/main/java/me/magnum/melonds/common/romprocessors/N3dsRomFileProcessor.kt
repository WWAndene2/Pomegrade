package me.magnum.melonds.common.romprocessors

import android.content.Context
import android.graphics.Bitmap
import android.net.Uri
import me.magnum.melonds.R
import me.magnum.melonds.common.uridelegates.UriHandler
import me.magnum.melonds.domain.model.RomInfo
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.config.RomConfig
import me.magnum.melonds.extensions.nameWithoutExtension
import me.magnum.melonds.impl.PomegradeFolder
import org.citra.citra_emu.CitraApplication
import org.citra.citra_emu.NativeLibrary
import org.citra.citra_emu.model.GameInfo
import java.nio.IntBuffer

/**
 * 3DS games, run by the Azahar core. Their title, publisher and icon are read with Azahar's
 * GameInfo (the game's SMDH), by file path: it needs "All files access" (see PomegradeFolder) and a
 * 64-bit device; without them the game is listed by its file name, with no icon.
 */
class N3dsRomFileProcessor(private val context: Context, private val uriHandler: UriHandler) : RomFileProcessor {

    companion object {
        // the 3DS icon: 48x48, RGB565 pixels packed two per Int (as Azahar's GameIconFetcher reads it)
        private const val ICON_SIZE = 48
    }

    override fun getRomFromUri(romUri: Uri, parentUri: Uri?): Rom? {
        val romDocument = uriHandler.getUriDocument(romUri) ?: return null
        val info = gameInfo(romUri)
        val title = info?.getTitle()?.replace(Regex("[\\t\\n\\r]+"), " ")?.trim()
        val publisher = when {
            info == null -> ""
            info.isEncrypted() -> context.getString(R.string.three_ds_encrypted)
            else -> info.getCompany().trim()
        }
        return Rom(
            name = title?.takeIf { it.isNotEmpty() } ?: romDocument.nameWithoutExtension ?: "",
            developerName = publisher,
            fileName = romDocument.name ?: "",
            uri = romUri,
            parentTreeUri = parentUri,
            config = RomConfig.default(),
            lastPlayed = null,
            isDsiWareTitle = false,
            retroAchievementsHash = "",
        )
    }

    override fun getRomIcon(rom: Rom): Bitmap? {
        val pixels = gameInfo(rom.uri)?.getIcon() ?: return null
        if (pixels.size * 2 < ICON_SIZE * ICON_SIZE) return null
        return Bitmap.createBitmap(ICON_SIZE, ICON_SIZE, Bitmap.Config.RGB_565).apply {
            copyPixelsFromBuffer(IntBuffer.wrap(pixels))
        }
    }

    override fun getRomInfo(rom: Rom): RomInfo? = null

    override suspend fun getRealRomUri(rom: Rom): Uri = rom.uri

    // the game's file read by the 3DS core, or null (32-bit device, no file access, not a file of
    // the phone's storage, or a file the core can't read)
    private fun gameInfo(uri: Uri): GameInfo? {
        if (!CitraApplication.isSupported || !PomegradeFolder.hasFileAccess(context)) return null
        val documentId = PomegradeFolder.documentIdOf(context, uri) ?: return null
        val path = PomegradeFolder.pathOf(documentId)
        if (!path.isFile) return null
        return try {
            // loads the 3DS core's native library (NativeLibrary's initializer), and starts its
            // runtime: GameInfo looks for the game's update in the 3DS folder (its icon and title),
            // through Azahar's document tree, created by CitraApplication.start
            NativeLibrary.toString()
            CitraApplication.start()
            // "!": a file path, not a document URI (as Azahar's GameHelper passes it)
            GameInfo("!" + path.absolutePath).takeIf { it.isValid() }
        } catch (e: Throwable) {
            null
        }
    }
}
