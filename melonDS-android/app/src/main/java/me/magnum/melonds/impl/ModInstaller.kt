package me.magnum.melonds.impl

import java.io.File
import java.io.IOException
import java.io.InputStream
import java.security.DigestInputStream
import java.security.MessageDigest
import java.util.zip.ZipInputStream

/**
 * Installs a 3DS mod from a zip into the 3DS folder (Punica/3DS, PunicaLayout), as the Remake mod workflow ships
 * them, each part where Azahar reads it there:
 *
 * ```
 * load/mods/<program id>/romfs/…, romfs_ext/….bps, exefs/…   the mod (Azahar's LayeredFS) -> 3DS/Mods/<program id>/…
 * sdmc/Nintendo 3DS/…/title/…/data/…                        a save that goes with it      -> 3DS/System/sdmc/…
 * ```
 *
 * A 3DS folder not in that layout yet (no Core and System folders, the test FileUtil::SetUserPath makes in
 * azahar/src/common/file_util.cpp) gets them as Azahar reads them there: 3DS/load/… and 3DS/sdmc/….
 *
 * A zip holding those two folders inside one top folder (a zip of the folder rather than of its
 * content) is read the same way. Each game's previous mod is deleted first, in both places Azahar may
 * read it (Mods/<program id>, load/mods/<program id>, and Mods/mods/<program id> where earlier versions put it: the
 * core picks one when it starts, so a
 * copy left in the other could still be loaded): a file of an older mod left in place would still be
 * loaded. Files of a save replace the ones there. Entries outside load/mods and sdmc are skipped.
 *
 * Installs are recorded ([RECORD], in the mods folder: game, mod name, the zip's SHA-256, file count, date),
 * so a zip is recognised before it is installed ([recognise]): a new mod for the game, the same zip again,
 * or an update of another mod, with the copies of a mod found for the game. Plain java.io, so it runs (and
 * is tested) off Android; call it on a background thread.
 */
class ModInstaller {

    /**
     * What a zip holds: the games it mods (program IDs), how many save files it brings, how many
     * entries are skipped, and the top folder its content is in ("" when at the zip's root).
     */
    data class Contents(val programIds: Set<String>, val saveFiles: Int, val skipped: Int, val topFolder: String, val sha256: String = "") {
        val isMod get() = programIds.isNotEmpty() || saveFiles > 0
    }

    /** A game's mod as the record has it: its name (the zip's), the zip's SHA-256, its file count, when (ms). */
    data class InstalledMod(val programId: String, val name: String, val sha256: String, val files: Int, val installedAt: Long)

    /**
     * What installing a zip does to one game it mods: [previous] the mod the record has for it (null: none
     * recorded), [sameZip] that mod is this very zip, [copies] the folders holding a mod for it now (relative to
     * the 3DS folder), all deleted by the install.
     */
    data class Recognition(val programId: String, val previous: InstalledMod?, val sameZip: Boolean, val copies: List<String>)

    sealed class Result {
        /** [removed]: the folders of earlier mods deleted, relative to the 3DS folder. */
        data class Installed(val programIds: Set<String>, val files: Int, val saveFiles: Int, val removed: List<String> = emptyList()) : Result()
        /** Neither load/mods/<program id>/ nor sdmc/ in the zip. */
        data object NotAMod : Result()
        data object Failed : Result()
    }

    /** Reads the zip [open] gives (opened once) without writing anything. */
    fun inspect(open: () -> InputStream): Contents {
        val entries = ArrayList<String>()
        val digest = MessageDigest.getInstance("SHA-256")
        DigestInputStream(open(), digest).use { stream ->
            ZipInputStream(stream).let { zip ->
                while (true) {
                    val entry = zip.nextEntry ?: break
                    if (!entry.isDirectory) entries.add(entry.name)
                }
            }
            // the zip's end (its central directory) too, so the fingerprint covers the whole file
            stream.readBytes()
        }
        return contentsOf(entries).copy(sha256 = digest.digest().joinToString("") { "%02x".format(it) })
    }

    /** What installing [contents] into [threeDsFolder] would do, game by game (nothing is written). */
    fun recognise(contents: Contents, threeDsFolder: File): List<Recognition> {
        val root = threeDsFolder.canonicalFile
        val record = readRecord(root)
        return contents.programIds.sorted().map { id ->
            val previous = record[id]
            Recognition(id, previous, previous != null && previous.sha256 == contents.sha256, modCopies(root, id).map { it.relativeTo(root).path })
        }
    }

    /**
     * Installs the zip [open] gives (opened twice: read, then extracted) into [threeDsFolder].
     * A failure halfway leaves what was extracted so far; installing again replaces it.
     */
    fun install(open: () -> InputStream, threeDsFolder: File, name: String = "", now: Long = System.currentTimeMillis()): Result {
        val contents = try {
            inspect(open)
        } catch (e: IOException) {
            return Result.Failed
        }
        if (!contents.isMod) return Result.NotAMod
        return try {
            val root = threeDsFolder.canonicalFile
            val layout = isLayout(root)
            val removed = ArrayList<String>()
            for (id in contents.programIds) {
                for (copy in modCopies(root, id)) {
                    if (!copy.deleteRecursively()) return Result.Failed
                    removed.add(copy.relativeTo(root).path)
                }
            }
            var files = 0
            val prefix = contents.topFolder
            ZipInputStream(open()).use { zip ->
                while (true) {
                    val entry = zip.nextEntry ?: break
                    if (entry.isDirectory) continue
                    val path = relativePath(entry.name, prefix) ?: continue
                    val target = File(root, placed(path, layout)).canonicalFile
                    // a name with ".." or a link out of the folder ("zip slip") is never written
                    if (!target.path.startsWith(root.path + File.separator)) continue
                    target.parentFile?.let { if (!it.isDirectory && !it.mkdirs()) throw IOException("can't create $it") }
                    target.outputStream().use { zip.copyTo(it) }
                    files++
                }
            }
            if (contents.programIds.isNotEmpty()) {
                val record = readRecord(root).toMutableMap()
                for (id in contents.programIds) record[id] = InstalledMod(id, name, contents.sha256, files, now)
                writeRecord(root, layout, record.values)
            }
            Result.Installed(contents.programIds, files, contents.saveFiles, removed)
        } catch (e: IOException) {
            Result.Failed
        }
    }

    // the 3DS folder is in Punica's layout (as FileUtil::SetUserPath tells it: Core and System folders)
    private fun isLayout(root: File) = File(root, PunicaLayout.CORE).isDirectory && File(root, PunicaLayout.SYSTEM).isDirectory

    // a game's mod in every folder Azahar may read it from, whichever layout it started with
    private fun modCopies(root: File, programId: String): List<File> =
        listOf("${PunicaLayout.MODS}/$programId", "${PunicaLayout.MODS}/mods/$programId", "$LOAD/mods/$programId").map { File(root, it) }.filter { it.exists() }

    // the record, in the mods folder of either layout (the current one first): game -> its mod
    private fun readRecord(root: File): Map<String, InstalledMod> {
        val file = listOf(File(root, placed("$LOAD/$RECORD", isLayout(root))), File(root, placed("$LOAD/$RECORD", !isLayout(root))))
            .firstOrNull { it.isFile } ?: return emptyMap()
        return try {
            file.readLines().mapNotNull { line ->
                val f = line.split('\t')
                if (f.size < 5) null else InstalledMod(f[0], f[1], f[2], f[3].toIntOrNull() ?: 0, f[4].toLongOrNull() ?: 0L)
            }.associateBy { it.programId }
        } catch (e: IOException) {
            emptyMap()
        }
    }

    // one line per game: program ID, name, SHA-256, files, time (tabs and line ends taken out of the name); a record
    // left in the other layout's folder is removed, so only the current one is read
    private fun writeRecord(root: File, layout: Boolean, mods: Collection<InstalledMod>) {
        val file = File(root, placed("$LOAD/$RECORD", layout))
        file.parentFile?.let { if (!it.isDirectory && !it.mkdirs()) throw IOException("can't create $it") }
        file.writeText(mods.sortedBy { it.programId }.joinToString("") {
            "${it.programId}\t${it.name.replace(Regex("[\t\r\n]"), " ")}\t${it.sha256}\t${it.files}\t${it.installedAt}\n"
        })
        File(root, placed("$LOAD/$RECORD", !layout)).takeIf { it.isFile }?.delete()
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

    // a zip path (load/…, sdmc/…) to its place in the 3DS folder: in the layout ([layout]) a game's mod is
    // Mods/<program id> (Azahar's FileUtil::GetModDir there), the rest of load in Mods, the SD card System/sdmc;
    // otherwise both are where the zip has them
    private fun placed(path: String, layout: Boolean): String = when {
        !layout -> path
        path.startsWith("$LOAD/mods/") -> "${PunicaLayout.MODS}/" + path.removePrefix("$LOAD/mods/")
        path.startsWith("$LOAD/") -> "${PunicaLayout.MODS}/" + path.removePrefix("$LOAD/")
        path.startsWith("$SDMC/") -> "${PunicaLayout.SYSTEM}/" + path
        else -> path
    }

    private companion object {
        const val LOAD = "load"
        const val SDMC = "sdmc"
        // the installs' record, in the mods folder beside Azahar's mods/ (which it alone reads)
        const val RECORD = "punica_installed_mods.tsv"
        // a 3DS title's program ID, as Azahar names a mod's folder (000400000011C400: Omega Ruby)
        val PROGRAM_ID = Regex("[0-9A-Fa-f]{16}")
    }
}
