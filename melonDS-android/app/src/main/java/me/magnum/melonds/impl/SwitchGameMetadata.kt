package me.magnum.melonds.impl

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Looper
import android.os.SystemClock
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
 * RomIconProvider, so each is read once. Eden's metadata cache is not thread-safe: every call into it holds [lock].
 * A file Eden can't open (getIsValid false) is never asked for its title or icon: Eden's native code would read a
 * null loader and crash the app. Without keys the core is prepared again at most once a minute (the owner may add
 * them to BIOS/Switch while the app runs).
 */
object SwitchGameMetadata {

    private const val TAG = "SwitchGameMetadata"
    private const val RETRY_MS = 60_000L
    private val lock = Any()
    private var preparedAt = 0L
    private var keysPresent = false

    /** The game's title, or null when it can't be read (no keys, not a game, on the main thread). */
    fun title(context: Context, uri: Uri): String? = withCore(context, uri) {
        GameMetadata.getTitle(it).trim().takeIf { title -> title.isNotEmpty() }
    }

    /** The game's icon, or null. */
    fun icon(context: Context, uri: Uri): Bitmap? = withCore(context, uri) {
        val data = GameMetadata.getIcon(it)
        if (data.isEmpty()) null else BitmapFactory.decodeByteArray(data, 0, data.size)
    }

    // runs [read] on a game Eden can open, one call into Eden at a time, or gives null
    private fun <T> withCore(context: Context, uri: Uri, read: (String) -> T?): T? {
        if (!YuzuApplication.isSupported || Looper.myLooper() == Looper.getMainLooper()) return null
        synchronized(lock) {
            if (!ready(context)) return null
            val path = uri.toString()
            return runCatching { if (GameMetadata.getIsValid(path)) read(path) else null }.getOrNull()
        }
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

    // under [lock]
    private fun ready(context: Context): Boolean {
        if (keysPresent) return true
        val now = SystemClock.elapsedRealtime()
        if (preparedAt != 0L && now - preparedAt < RETRY_MS) return false
        preparedAt = now
        runCatching {
            runBlocking { withContext(Dispatchers.Main) { SwitchLauncher.prepare(context) } }
        }.onFailure { Log.w(TAG, "Switch core not started for the game list", it) }
        keysPresent = runCatching { NativeLibrary.areKeysPresent() }.getOrDefault(false)
        return keysPresent
    }
}
