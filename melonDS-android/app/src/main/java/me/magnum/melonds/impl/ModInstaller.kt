package me.magnum.melonds.impl

import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.zip.ZipInputStream

/**
 * Installs a 3DS mod from a zip into the 3DS core's folder (Azahar's user folder, Pomegrade/3DS),
 * as the Remake mod workflow ships them:
 *
 * ```
 * load/mods/<program id>/romfs/…, romfs_ext/….bps, exefs/…   the mod, read by Azahar's LayeredFS
 * sdmc/Nintendo 3DS/…/title/…/data/…                        a save that goes with it (optional)
 * ```
 *
 * A zip holding those two folders inside one top folder (a zip of the folder rather than of its
 * content) is read the same way. Each game's previous mod (load/mods/<program id>) is deleted
 * first: a file of an older mod left in place would still be loaded. Files of a save replace the
 * ones there. Entries outside load/mods and sdmc are skipped. Plain java.io, so it runs (and is
 * tested) off Android; call it on a background thread.
 */
class ModInstaller {

    /**
     * What a zip holds: the games it mods (program IDs), how many save files it brings, how many
     * entries are skipped, and the top folder its content is in ("" when at the zip's root).
     */
    data class Contents(val programIds: Set<String>, val saveFiles: Int, val skipped: Int, val topFolder: String) {
        val isMod get() = programIds.isNotEmpty() || saveFiles > 0
    }

    sealed class Result {
        data class Installed(val programIds: Set<String>, val files: Int, val saveFiles: Int) : Result()
        /** Neither load/mods/<program id>/ nor sdmc/ in the zip. */
        data object NotAMod : Result()
        data object Failed : Result()
    }

    /** Reads the zip [open] gives (opened once) without writing anything. */
    fun inspect(open: () -> InputStream): Contents {
        val entries = ArrayList<String>()
        ZipInputStream(open()).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (!entry.isDirectory) entries.add(entry.name)
            }
        }
        return contentsOf(entries)
    }

    /**
     * Installs the zip [open] gives (opened twice: read, then extracted) into [threeDsFolder].
     * A failure halfway leaves what was extracted so far; installing again replaces it.
     */
    fun install(open: () -> InputStream, threeDsFolder: File): Result {
        val contents = try {
            inspect(open)
        } catch (e: IOException) {
            return Result.Failed
        }
        if (!contents.isMod) return Result.NotAMod
        return try {
            val root = threeDsFolder.canonicalFile
            for (id in contents.programIds) {
                val previous = File(root, "load/mods/$id")
                if (previous.exists() && !previous.deleteRecursively()) return Result.Failed
            }
            var files = 0
            val prefix = contents.topFolder
            ZipInputStream(open()).use { zip ->
                while (true) {
                    val entry = zip.nextEntry ?: break
                    if (entry.isDirectory) continue
                    val path = relativePath(entry.name, prefix) ?: continue
                    val target = File(root, path).canonicalFile
                    // a name with ".." or a link out of the folder ("zip slip") is never written
                    if (!target.path.startsWith(root.path + File.separator)) continue
                    target.parentFile?.let { if (!it.isDirectory && !it.mkdirs()) throw IOException("can't create $it") }
                    target.outputStream().use { zip.copyTo(it) }
                    files++
                }
            }
            Result.Installed(contents.programIds, files, contents.saveFiles)
        } catch (e: IOException) {
            Result.Failed
        }
    }

    private fun contentsOf(names: List<String>): Contents {
        val prefix = topFolder(names)
        val ids = HashSet<String>()
        var save = 0
        var skipped = 0
        for (name in names) {
            val path = relativePath(name, prefix)
            when {
                path == null -> skipped++
                path.startsWith("$SDMC/") -> save++
                else -> ids.add(path.split('/')[2])
            }
        }
        return Contents(ids, save, skipped, prefix)
    }

    // a zip of the mod's folder rather than of its content: every entry under one folder that
    // holds load/ or sdmc/
    private fun topFolder(names: List<String>): String {
        val first = names.firstOrNull()?.substringBefore('/', "") ?: return ""
        if (first.isEmpty() || first == LOAD || first == SDMC) return ""
        val prefix = "$first/"
        val inside = names.all { it.startsWith(prefix) } &&
            names.any { it.startsWith("$prefix$LOAD/") || it.startsWith("$prefix$SDMC/") }
        return if (inside) prefix else ""
    }

    // the entry's path inside the 3DS folder, or null for an entry that isn't a mod's or a save's
    private fun relativePath(name: String, prefix: String): String? {
        val path = name.replace('\\', '/').removePrefix(prefix)
        val parts = path.split('/')
        return when {
            parts.any { it == ".." } -> null
            parts[0] == SDMC && parts.size > 1 -> path
            // load/mods/<program id>/<file or folder>
            parts[0] == LOAD && parts.size > 3 && parts[1] == "mods" && PROGRAM_ID.matches(parts[2]) -> path
            else -> null
        }
    }

    private companion object {
        const val LOAD = "load"
        const val SDMC = "sdmc"
        // a 3DS title's program ID, as Azahar names a mod's folder (000400000011C400: Omega Ruby)
        val PROGRAM_ID = Regex("[0-9A-Fa-f]{16}")
    }
}
