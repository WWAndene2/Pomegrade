package me.magnum.melonds.impl

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Looper
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withContext
import me.magnum.melonds.ui.common.rom.SwitchLauncher
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.utils.GameMetadata

/**
 * A Switch game's real title and icon for Punica's game list (owner's request). Both are encrypted in the game's
 * file: reading them needs the Switch core started with the owner's keys, as Eden's own game list does. The core
 * is started the first time a Switch game is listed, as launching a game does ([SwitchLauncher.prepare]: its
 * folder, then the keys and firmware of BIOS/Switch), on a background thread. Without keys, or on a phone the core
 * does not run on, the title is the file's name without its tags and there is no icon.
 *
 * Called from the game scan and the icon loader, both off the main thread; the icons are then kept by
 * RomIconProvider, so each is read once.
 */
object SwitchGameMetadata {

    private const val TAG = "SwitchGameMetadata"
    private val lock = Any()
    private var prepared = false

    /** The game's title, or null when it can't be read (no keys, not a game, on the main thread). */
    fun title(context: Context, uri: Uri): String? {
        if (!ready(context)) return null
        return runCatching { GameMetadata.getTitle(uri.toString()) }.getOrNull()?.trim()?.takeIf { it.isNotEmpty() }
    }

    /** The game's icon, or null. */
    fun icon(context: Context, uri: Uri): Bitmap? {
        if (!ready(context)) return null
        val data = runCatching { GameMetadata.getIcon(uri.toString()) }.getOrNull() ?: return null
        if (data.isEmpty()) return null
        return BitmapFactory.decodeByteArray(data, 0, data.size)
    }

    /**
     * A file name as a title when the real one can't be read: without its extension and its dump tags
     * ("Pokemon Sword [0100ABF008968000][v0].nsp" -> "Pokemon Sword").
     */
    fun cleanFileName(fileName: String): String {
        val base = fileName.substringBeforeLast('.')
        val clean = base.replace(Regex("""\s*[\[(][^\])]*[\])]"""), "").replace('_', ' ').trim()
        return clean.ifEmpty { base }
    }

    private fun ready(context: Context): Boolean {
        if (!YuzuApplication.isSupported || Looper.myLooper() == Looper.getMainLooper()) return false
        synchronized(lock) {
            if (!prepared) {
                runCatching {
                    runBlocking { withContext(Dispatchers.Main) { SwitchLauncher.prepare(context) } }
                }.onFailure { Log.w(TAG, "Switch core not started for the game list", it) }
                prepared = true
            }
        }
        return runCatching { NativeLibrary.areKeysPresent() }.getOrDefault(false)
    }
}
