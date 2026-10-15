package me.magnum.melonds.impl

import android.content.Context
import android.net.Uri
import androidx.preference.PreferenceManager
import me.magnum.melonds.domain.model.rom.Rom
import me.magnum.melonds.domain.model.rom.RomPlatform
import java.io.File
import java.io.InputStream
import java.util.zip.ZipInputStream

/**
 * Installs a texture pack (a zip) for one game, into the folder its console's core reads, named after the
 * game's ID:
 * - DS: Textures/<game code> (4 characters, the cartridge header's), read by melonDS's texture replacement;
 * - 3DS: 3DS/load/textures/<title ID> (16 hexadecimal digits), Azahar's custom textures;
 * - GameCube / Wii: GC-Wii/Load/Textures/<game ID> (6 characters, the disc header's), Dolphin's.
 * The ID is read from the game's file; when the format hides it (a 3DS CIA or 3DSX, a GCZ or CISO disc),
 * the pack's own top folder gives it, as packs are usually made (GALE01/..., 0004000000030800/...). A pack
 * held in one top folder has it removed, whatever its name. Files already there with the same names are
 * replaced; the others stay. The Switch and GBA cores have no texture packs. Background thread.
 */
object TexturePackInstaller {

    sealed class Result {
        /** [files] written into [folder]; [enableHint] is true when the core's setting must be turned on. */
        data class Installed(val files: Int, val folder: File, val enableHint: Boolean) : Result()
        data object NoGameId : Result()
        data object NoFolder : Result()
        data object Empty : Result()
        data class Failed(val error: Throwable) : Result()
    }

    fun isSupported(platform: RomPlatform) = platform == RomPlatform.NDS || platform == RomPlatform.N3DS || platform == RomPlatform.GC_WII

    fun install(context: Context, rom: Rom, pack: Uri): Result = try {
        val names = ZipInputStream(context.contentResolver.openInputStream(pack)!!.buffered()).use { zip ->
            generateSequence { zip.nextEntry }.filterNot { it.isDirectory }.map { it.name }.toList()
        }
        val layout = layout(names, rom.platform)
        val id = gameId(context, rom) ?: layout.folderId
        when {
            layout.files.isEmpty() -> Result.Empty
            id == null -> Result.NoGameId
            else -> {
                val root = texturesRoot(context, rom.platform)
                if (root == null) Result.NoFolder else write(context, rom.platform, pack, layout, File(root, id))
            }
        }
    } catch (e: Exception) {
        Result.Failed(e)
    }

    private fun write(context: Context, platform: RomPlatform, pack: Uri, layout: Layout, folder: File): Result {
        val target = folder.canonicalFile
        var written = 0
        ZipInputStream(context.contentResolver.openInputStream(pack)!!.buffered()).use { zip ->
            generateSequence { zip.nextEntry }.filterNot { it.isDirectory }.forEach { entry ->
                val relative = layout.files[entry.name] ?: return@forEach
                val to = File(target, relative).canonicalFile
                // an entry may not write outside the game's folder ("../")
                if (!to.path.startsWith(target.path + File.separator)) return@forEach
                to.parentFile?.mkdirs()
                to.outputStream().use { zip.copyTo(it) }
                written++
            }
        }
        if (written == 0) return Result.Empty
        // Punica's own setting: turned on, the pack is what was asked for. The 3DS and GameCube / Wii
        // settings are the cores' own (their files), left to the user, who is told where
        val enableHint = if (platform == RomPlatform.NDS) {
            PreferenceManager.getDefaultSharedPreferences(context).edit().putBoolean(DS_SETTING, true).apply()
            false
        } else {
            true
        }
        return Result.Installed(written, target, enableHint)
    }

    private fun texturesRoot(context: Context, platform: RomPlatform): File? = when (platform) {
        // each console's Mods (PunicaLayout), in the form its core reads: DS <game code>/, 3DS (Azahar's load folder)
        // textures/<title id>/, GameCube / Wii (Dolphin's Load folder) Textures/<game id>/
        RomPlatform.NDS -> PomegradeFolder.consoleFolder(context, PunicaLayout.DS, PunicaLayout.MODS)
        RomPlatform.N3DS -> PomegradeFolder.consoleFolder(context, PunicaLayout.N3DS, PunicaLayout.MODS)?.let { File(it, "textures") }
        RomPlatform.GC_WII -> PomegradeFolder.consoleFolder(context, PunicaLayout.GC_WII, PunicaLayout.MODS)?.let { File(it, "Textures") }
        else -> null
    }?.takeIf { it.isDirectory || it.mkdirs() }

    private const val DS_SETTING = "enable_hd_textures"

    /** The pack's files ([files]: zip entry → path in the game's folder) and its top folder's ID, if it is one. */
    internal data class Layout(val files: Map<String, String>, val folderId: String?)

    internal fun layout(entries: List<String>, platform: RomPlatform): Layout {
        val names = entries.map { it.replace('\\', '/').trimStart('/') }.filter { it.isNotEmpty() && !isJunk(it) }
        val tops = names.map { it.substringBefore('/', "") }.toSet()
        // one top folder for every file: removed, and its name is the ID when it has an ID's form
        val top = tops.singleOrNull()?.takeIf { it.isNotEmpty() }
        val files = entries.zip(entries.map { it.replace('\\', '/').trimStart('/') })
            .filter { (_, name) -> name.isNotEmpty() && !isJunk(name) }
            .associate { (entry, name) -> entry to (if (top != null) name.removePrefix("$top/") else name) }
        return Layout(files, top?.takeIf { idForm(platform).matches(it) }?.uppercase())
    }

    // what archivers add (macOS's resource forks, Windows's thumbnails)
    private fun isJunk(name: String) = name.startsWith("__MACOSX/") || name.substringAfterLast('/') in setOf(".DS_Store", "Thumbs.db")

    private fun idForm(platform: RomPlatform) = when (platform) {
        RomPlatform.NDS -> Regex("[A-Za-z0-9]{4}")
        RomPlatform.N3DS -> Regex("[0-9A-Fa-f]{16}")
        else -> Regex("[A-Za-z0-9]{6}")
    }

    /** The game's ID as its core names its textures folder, read from its file; null when the format hides it. */
    fun gameId(context: Context, rom: Rom): String? = runCatching {
        context.contentResolver.openInputStream(rom.uri)?.use { stream ->
            val extension = rom.fileName.substringAfterLast('.', "").lowercase()
            when (rom.platform) {
                RomPlatform.NDS -> ascii(read(stream, 0x0C, 4))
                RomPlatform.N3DS -> when (extension) {
                    // NCSD: the media ID, the title's; NCCH: the program ID
                    "3ds", "cci" -> titleId(read(stream, 0x108, 8))
                    "cxi" -> titleId(read(stream, 0x118, 8))
                    else -> null
                }
                RomPlatform.GC_WII -> discId(stream, extension)
                else -> null
            }
        }
    }.getOrNull()

    private fun discId(stream: InputStream, extension: String): String? = when (extension) {
        "iso", "gcm", "tgc" -> ascii(read(stream, 0, 6))
        // WIA and RVZ: a copy of the disc header after the two file headers (0x48 + 0x10 bytes)
        "wia", "rvz" -> ascii(read(stream, 0x58, 6))
        // WBFS: the disc header in the second block, of the size the header gives (2^byte 8)
        "wbfs" -> {
            val header = read(stream, 0, 9) ?: return null
            val blockSize = 1L shl header[8].toInt()
            if (blockSize !in 512L..65536L) null else ascii(read(stream, blockSize - 9, 6))
        }
        else -> null
    }

    // [length] bytes at [offset] from the stream's current place, or null when the file is shorter
    private fun read(stream: InputStream, offset: Long, length: Int): ByteArray? {
        var skip = offset
        while (skip > 0) {
            val skipped = stream.skip(skip)
            if (skipped <= 0) return null
            skip -= skipped
        }
        val bytes = ByteArray(length)
        var read = 0
        while (read < length) {
            val n = stream.read(bytes, read, length - read)
            if (n < 0) return null
            read += n
        }
        return bytes
    }

    // letters and digits only, as the cores' IDs are: anything else isn't a header
    private fun ascii(bytes: ByteArray?): String? =
        bytes?.toString(Charsets.US_ASCII)?.takeIf { id -> id.all { it.isLetterOrDigit() && it.code < 128 } }

    private fun titleId(bytes: ByteArray?): String? {
        bytes ?: return null
        val value = bytes.foldIndexed(0L) { i, acc, b -> acc or ((b.toLong() and 0xFF) shl (8 * i)) }
        return if (value == 0L) null else "%016X".format(value)
    }
}
