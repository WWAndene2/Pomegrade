package me.magnum.melonds.impl

import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.zip.ZipInputStream

/**
 * Installs a Switch mod from a zip into the Switch mods folder (Punica/Switch/Mods, Eden's load directory), where
 * Eden reads a game's mods: one folder per title ID, then one per mod, holding exefs/, romfs/, cheats/…
 * (eden/src/core/file_sys/patch_manager.cpp). Two zip layouts are read, the way Switch mods are shared:
 *
 * ```
 * <title id>/<mod name>/exefs/…, romfs/…                 Eden's and yuzu's layout   -> Mods/<title id>/<mod name>/…
 * atmosphere/contents/<title id>/exefs/…, romfs/…        Atmosphère's layout        -> Mods/<title id>/<zip name>/…
 * ```
 *
 * Either may sit inside one top folder (a zip of the folder rather than of its content). A game can have several
 * mods at once, each turned on or off in Eden's add-ons: an install replaces only the mod of the same name for that
 * game (its folder deleted first, so no file of an older version stays), never the game's other mods. Entries
 * outside these layouts are skipped. Plain java.io, so it runs (and is tested) off Android; call it on a background
 * thread.
 */
class SwitchModInstaller {

    /** One mod in a zip: the game's title ID and the mod's name (its folder in Mods/<title id>/). */
    data class Mod(val titleId: String, val name: String)

    /**
     * What a zip holds: its mods, how many files they have, how many entries are skipped, and the top folder its
     * content is in ("" when at the zip's root).
     */
    data class Contents(val mods: Set<Mod>, val files: Int, val skipped: Int, val topFolder: String = "") {
        val isMod get() = mods.isNotEmpty()
    }

    sealed class Result {
        /** [replaced]: the mods that were there before, under the same names, and were replaced. */
        data class Installed(val mods: Set<Mod>, val files: Int, val replaced: Set<Mod>) : Result()
        data object NotAMod : Result()
        data object Failed : Result()
    }

    /** Reads the zip [open] gives without writing anything; [zipName] names an Atmosphère-layout mod. */
    fun inspect(open: () -> InputStream, zipName: String): Contents {
        val names = ArrayList<String>()
        ZipInputStream(open()).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (!entry.isDirectory) names.add(entry.name.replace('\\', '/'))
            }
        }
        val prefix = topFolder(names)
        val mods = HashSet<Mod>()
        var files = 0
        var skipped = 0
        for (name in names) {
            val placed = place(name.removePrefix(prefix), zipName)
            if (placed == null) skipped++ else { mods.add(placed.first); files++ }
        }
        return Contents(mods, files, skipped, prefix)
    }

    /** Which of [contents]' mods are already in [modsFolder] under the same name (replaced by an install). */
    fun alreadyInstalled(contents: Contents, modsFolder: File): Set<Mod> =
        contents.mods.filter { File(modsFolder, "${it.titleId}/${it.name}").exists() }.toSet()

    /**
     * Installs the zip [open] gives (opened twice: read, then extracted) into [modsFolder].
     * A failure halfway leaves what was extracted so far; installing again replaces it.
     */
    fun install(open: () -> InputStream, modsFolder: File, zipName: String): Result {
        val contents = try {
            inspect(open, zipName)
        } catch (e: IOException) {
            return Result.Failed
        }
        if (!contents.isMod) return Result.NotAMod
        return try {
            val root = modsFolder.canonicalFile
            val replaced = alreadyInstalled(contents, root)
            for (mod in replaced) {
                if (!File(root, "${mod.titleId}/${mod.name}").deleteRecursively()) return Result.Failed
            }
            var files = 0
            ZipInputStream(open()).use { zip ->
                val prefix = contents.topFolder
                while (true) {
                    val entry = zip.nextEntry ?: break
                    if (entry.isDirectory) continue
                    val placed = place(entry.name.replace('\\', '/').removePrefix(prefix), zipName) ?: continue
                    val target = File(root, placed.second).canonicalFile
                    // a name with ".." or a link out of the folder ("zip slip") is never written
                    if (!target.path.startsWith(root.path + File.separator)) continue
                    target.parentFile?.let { if (!it.isDirectory && !it.mkdirs()) throw IOException("can't create $it") }
                    target.outputStream().use { zip.copyTo(it) }
                    files++
                }
            }
            Result.Installed(contents.mods, files, replaced)
        } catch (e: IOException) {
            Result.Failed
        }
    }

    // a zip of the mod's folder rather than of its content: every entry under one folder that is neither a title
    // ID nor "atmosphere"
    private fun topFolder(names: List<String>): String {
        val first = names.firstOrNull()?.substringBefore('/', "") ?: return ""
        if (first.isEmpty() || TITLE_ID.matches(first) || first.equals(ATMOSPHERE, ignoreCase = true)) return ""
        val prefix = "$first/"
        return if (names.all { it.startsWith(prefix) }) prefix else ""
    }

    // a path in the zip (top folder removed) to its mod and its place in the mods folder, or null if it is neither
    // layout's
    private fun place(path: String, zipName: String): Pair<Mod, String>? {
        val parts = path.split('/')
        if (parts.any { it == ".." || it.isEmpty() }) return null
        // <title id>/<mod name>/<file or folder>
        if (parts.size > 2 && TITLE_ID.matches(parts[0])) {
            val mod = Mod(parts[0].uppercase(), parts[1])
            return mod to "${mod.titleId}/${mod.name}/" + parts.drop(2).joinToString("/")
        }
        // atmosphere/contents/<title id>/<file or folder>
        if (parts.size > 3 && parts[0].equals(ATMOSPHERE, ignoreCase = true) && parts[1].equals("contents", ignoreCase = true) &&
            TITLE_ID.matches(parts[2])) {
            val name = zipName.ifBlank { DEFAULT_NAME }.replace('/', ' ')
            val mod = Mod(parts[2].uppercase(), name)
            return mod to "${mod.titleId}/${mod.name}/" + parts.drop(3).joinToString("/")
        }
        return null
    }

    companion object {
        private const val ATMOSPHERE = "atmosphere"
        private const val DEFAULT_NAME = "Mod"
        // a Switch title ID, as Eden names a game's mods folder (0100A3D008C5C000: Pokemon Scarlet)
        private val TITLE_ID = Regex("[0-9A-Fa-f]{16}")
    }
}
