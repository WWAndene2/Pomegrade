package me.magnum.melonds.impl

import me.magnum.melonds.domain.model.rom.RomPlatform
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import java.util.zip.ZipOutputStream

/**
 * One game's saves and mods in the Punica folder (owner's request, 29 October: quick and simple management of each
 * game's mods and saves, from its sheet), where [PunicaLayout] puts them. Plain java.io: it runs (and is tested) off
 * Android; call it on a background thread.
 *
 * **Saves**, by console (paths in the console's folder):
 * - DS, GBA: Saves/<game file's name>.sav;
 * - 3DS: the game's folder on the emulated SD card, System/sdmc/Nintendo 3DS/<id0>/<id1>/title/<high>/<low>/data;
 * - Switch: its folder in Saves/user/save/<…>/<user>/<title id>, one per profile;
 * - GameCube: its GCI files in Saves/GameCube/Card A|B/<region>/ (<maker>-<game code>-….gci; a game on a whole
 *   memory card image shares it with every other game: not handled); Wii: System/Wii/title/00010000/<game code>/data.
 * A copy ([backup]) is a folder Backups/Saves/<console>/<game>/<date>/ holding the save files at their paths in the
 * console's folder, and a `.reason` (why it was made); [export] writes the same paths into a zip, [import] reads one
 * back. Restoring, deleting and importing first copy the save as it is, so every change can be undone.
 *
 * **Mods**, by console: a mod in use is where its core reads it (Mods/<game id>/ for the 3DS, one folder per mod in
 * Mods/<title id>/ for the Switch; texture packs Mods/Textures/<game id>/ for DS, 3DS and GameCube / Wii); one set
 * aside (turned off, or kept when another replaces it) is in Mods/Disabled/<the same path>/<its name>/. A 3DS game
 * reads one mod and one texture pack at a time: turning one on sets the other aside; Switch mods are each on or off.
 * A mod's name is in its folder's `.punica-name` (the zip's name it was installed from), else the folder's. Turning a
 * mod on or off, or installing one, first copies the save.
 */
class GameData(root: File, val platform: RomPlatform, fileName: String, private val gameId: String?) {

    val console = PunicaLayout.console(platform)
    private val consoleDir = File(root, console)
    private val baseName = fileName.substringBeforeLast('.', fileName)
    private val backupsDir = File(File(File(File(root, PunicaLayout.BACKUPS), "Saves"), console), safeName(baseName))

    data class Backup(val folder: File, val time: Long, val reason: String)

    enum class Kind { MOD, TEXTURES }

    data class Mod(val name: String, val kind: Kind, val active: Boolean, val folder: File)

    // ---- saves

    /** The game's save files and folders that exist now. */
    fun saves(): List<File> {
        val saves = consoleDir.resolve(PunicaLayout.SAVES)
        return when (platform) {
            RomPlatform.NDS, RomPlatform.GBA -> listOf(saves.resolve("$baseName.sav")).filter { it.isFile }
            RomPlatform.N3DS -> n3dsSaveFolders()
            RomPlatform.SWITCH -> {
                val id = gameId ?: return emptyList()
                // user/save/<…>/<user>/<title id>
                saves.resolve("user/save").listFiles().orEmpty().flatMap { it.listFiles().orEmpty().toList() }
                    .mapNotNull { user -> user.listFiles()?.firstOrNull { it.isDirectory && it.name.equals(id, ignoreCase = true) } }
            }
            RomPlatform.GC_WII -> {
                val id = gameId ?: return emptyList()
                val code = id.take(4)
                val gci = saves.resolve(PunicaLayout.GAMECUBE).walkTopDown()
                    .filter { it.isFile && it.name.endsWith(".gci", true) && it.name.contains("-$code-") }.toList()
                val wii = consoleDir.resolve("${PunicaLayout.SYSTEM}/Wii/title/00010000/${hexOf(code)}/data").takeIf { it.isDirectory }
                gci + listOfNotNull(wii)
            }
        }
    }

    /** When the save was last written, or null without one. */
    fun lastSaved(): Long? = saves().flatMap { save -> save.walkTopDown().filter { it.isFile }.toList() }.maxOfOrNull { it.lastModified() }

    /** The copies of the save, newest first. */
    fun backups(): List<Backup> = backupsDir.listFiles { f -> f.isDirectory }.orEmpty()
        .map { Backup(it, it.lastModified(), it.resolve(REASON).takeIf { r -> r.isFile }?.readText().orEmpty()) }
        .sortedByDescending { it.folder.name }

    /** Copies the save into a new backup folder (why: [reason]); null when there is no save. */
    fun backup(reason: String): Backup? {
        val saves = saves()
        if (saves.isEmpty()) return null
        var folder = backupsDir.resolve(STAMP.format(Date()))
        var n = 2
        while (folder.exists()) folder = backupsDir.resolve("${STAMP.format(Date())}-${n++}")
        saves.forEach { save -> save.copyRecursively(folder.resolve(save.relativeTo(consoleDir).path), overwrite = false) }
        folder.resolve(REASON).writeText(reason)
        return Backup(folder, folder.lastModified(), reason)
    }

    /** Puts [backup] back in place of the save, which is copied first. */
    fun restore(backup: Backup, reasonBefore: String) {
        backup(reasonBefore)
        saves().forEach { it.deleteRecursively() }
        backup.folder.listFiles { f -> f.name != REASON }.orEmpty().forEach { it.copyRecursively(consoleDir.resolve(it.name), overwrite = true) }
    }

    /** Deletes the save, copied first. */
    fun delete(reasonBefore: String) {
        backup(reasonBefore)
        saves().forEach { it.deleteRecursively() }
    }

    /** Deletes one copy. */
    fun deleteBackup(backup: Backup) {
        backup.folder.deleteRecursively()
    }

    /** Writes the save into a zip (its files at their paths in the console's folder); returns how many files. */
    fun export(out: OutputStream): Int {
        var files = 0
        ZipOutputStream(out).use { zip ->
            saves().forEach { save ->
                save.walkTopDown().filter { it.isFile }.forEach { file ->
                    zip.putNextEntry(ZipEntry(file.relativeTo(consoleDir).invariantSeparatorsPath))
                    file.inputStream().use { it.copyTo(zip) }
                    zip.closeEntry()
                    files++
                }
            }
        }
        return files
    }

    /**
     * Replaces the save with the one a zip holds (as [export] writes it), copying the save first. Only the entries
     * that are this game's save are written; returns how many, 0 when the zip holds none (nothing is changed then).
     */
    fun import(open: () -> InputStream, reasonBefore: String): Int {
        val entries = ZipInputStream(open()).use { zip -> generateSequence { zip.nextEntry }.filterNot { it.isDirectory }.map { it.name }.toList() }
        if (entries.none { isSavePath(it) }) return 0
        backup(reasonBefore)
        saves().forEach { it.deleteRecursively() }
        var written = 0
        val canonical = consoleDir.canonicalFile
        ZipInputStream(open()).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (entry.isDirectory || !isSavePath(entry.name)) continue
                val target = consoleDir.resolve(entry.name).canonicalFile
                // a name with ".." ("zip slip") is never written
                if (!target.path.startsWith(canonical.path + File.separator)) continue
                target.parentFile?.mkdirs()
                target.outputStream().use { zip.copyTo(it) }
                written++
            }
        }
        return written
    }

    // a path in the console's folder that is this game's save
    private fun isSavePath(path: String): Boolean {
        val p = path.replace('\\', '/')
        if (p.split('/').any { it == ".." }) return false
        val id = gameId
        return when (platform) {
            RomPlatform.NDS, RomPlatform.GBA -> p == "${PunicaLayout.SAVES}/$baseName.sav"
            RomPlatform.N3DS -> id != null && Regex("${PunicaLayout.SYSTEM}/sdmc/Nintendo 3DS/[^/]+/[^/]+/title/${id.take(8).lowercase()}/${id.drop(8).lowercase()}/data/.+").matches(p)
            RomPlatform.SWITCH -> id != null && Regex("${PunicaLayout.SAVES}/user/save/[^/]+/[^/]+/(?i:$id)/.+").matches(p)
            RomPlatform.GC_WII -> id != null && (
                Regex("${PunicaLayout.SAVES}/${PunicaLayout.GAMECUBE}/.+-${Regex.escape(id.take(4))}-[^/]*\\.gci", RegexOption.IGNORE_CASE).matches(p) ||
                    p.startsWith("${PunicaLayout.SYSTEM}/Wii/title/00010000/${hexOf(id.take(4))}/data/"))
        }
    }

    // the 3DS game's save folder on the emulated SD card, under any SD card ID
    private fun n3dsSaveFolders(): List<File> {
        val id = gameId ?: return emptyList()
        val sd = consoleDir.resolve("${PunicaLayout.SYSTEM}/sdmc/Nintendo 3DS")
        return sd.listFiles().orEmpty().flatMap { it.listFiles().orEmpty().toList() }
            .map { it.resolve("title/${id.take(8).lowercase()}/${id.drop(8).lowercase()}/data") }
            .filter { it.isDirectory }
    }

    // ---- mods

    /** The game's mods and texture packs, in use and set aside. */
    fun mods(): List<Mod> {
        val id = gameId ?: return emptyList()
        val mods = consoleDir.resolve(PunicaLayout.MODS)
        val disabled = mods.resolve(DISABLED)
        val list = ArrayList<Mod>()
        when (platform) {
            RomPlatform.N3DS -> {
                mods.resolve(id).takeIf { it.isDirectory }?.let { list += Mod(nameOf(it, recordName(id)), Kind.MOD, true, it) }
                disabled.resolve(id).listFiles { f -> f.isDirectory }.orEmpty().forEach { list += Mod(nameOf(it), Kind.MOD, false, it) }
            }
            RomPlatform.SWITCH -> {
                mods.resolve(id).listFiles { f -> f.isDirectory }.orEmpty().forEach { list += Mod(nameOf(it), Kind.MOD, true, it) }
                disabled.resolve(id).listFiles { f -> f.isDirectory }.orEmpty().forEach { list += Mod(nameOf(it), Kind.MOD, false, it) }
            }
            else -> Unit
        }
        if (platform in TEXTURE_PLATFORMS) {
            mods.resolve("${PunicaLayout.TEXTURES}/$id").takeIf { it.isDirectory }?.let { list += Mod(nameOf(it), Kind.TEXTURES, true, it) }
            disabled.resolve("${PunicaLayout.TEXTURES}/$id").listFiles { f -> f.isDirectory }.orEmpty().forEach { list += Mod(nameOf(it), Kind.TEXTURES, false, it) }
        }
        return list
    }

    /** Whether this console's games can have mods ([Kind.MOD]) and texture packs here. */
    val takesMods get() = platform == RomPlatform.N3DS || platform == RomPlatform.SWITCH
    val takesTextures get() = platform in TEXTURE_PLATFORMS

    /**
     * Turns [mod] on or off (the save copied first). On the 3DS (one mod, one texture pack at a time) and for texture
     * packs, turning one on sets the one in use aside.
     */
    fun setActive(mod: Mod, active: Boolean, reasonBefore: String) {
        if (mod.active == active) return
        val id = gameId ?: return
        backup(reasonBefore)
        if (!active) {
            setAside(mod)
            return
        }
        val mods = consoleDir.resolve(PunicaLayout.MODS)
        val place = when {
            mod.kind == Kind.TEXTURES -> mods.resolve("${PunicaLayout.TEXTURES}/$id")
            platform == RomPlatform.N3DS -> mods.resolve(id)
            else -> mods.resolve("$id/${mod.folder.name}")
        }
        // the one in use at that place set aside, kept
        mods().firstOrNull { it.active && it.folder == place }?.let { setAside(it) }
        writeName(mod.folder, mod.name)
        moveDir(mod.folder, place)
    }

    /** Deletes [mod], in use or set aside (the save copied first when it was in use). */
    fun deleteMod(mod: Mod, reasonBefore: String) {
        if (mod.active) backup(reasonBefore)
        mod.folder.deleteRecursively()
    }

    /**
     * Before a new mod or texture pack of [kind] is installed where the one in use is: that one set aside under its
     * name, so the new one replaces it without deleting it (the save copied first). Returns whether one was set aside.
     */
    fun setAsideBeforeInstall(kind: Kind, reasonBefore: String): Boolean {
        val current = mods().firstOrNull { it.active && it.kind == kind && (kind == Kind.TEXTURES || platform == RomPlatform.N3DS) } ?: run {
            backup(reasonBefore)
            return false
        }
        backup(reasonBefore)
        setAside(current)
        return true
    }

    /**
     * A Switch mod from a zip, as a new folder Mods/<title id>/<name>/ (on): its content as the zip has it (romfs/,
     * exefs/, cheats/), or the content of the zip's one top folder. Returns the files written (0: not a mod; nothing
     * written). The save copied first.
     */
    fun installSwitchMod(open: () -> InputStream, name: String, reasonBefore: String): Int {
        val id = gameId ?: return 0
        val names = ZipInputStream(open()).use { zip -> generateSequence { zip.nextEntry }.filterNot { it.isDirectory }.map { it.name.replace('\\', '/') }.toList() }
        val top = names.firstOrNull()?.substringBefore('/', "").orEmpty()
        val prefix = if (top.isNotEmpty() && top.lowercase() !in SWITCH_MOD_PARTS && names.all { it.startsWith("$top/") }) "$top/" else ""
        if (names.none { it.removePrefix(prefix).substringBefore('/').lowercase() in SWITCH_MOD_PARTS }) return 0
        backup(reasonBefore)
        var folder = consoleDir.resolve("${PunicaLayout.MODS}/$id/${safeName(name)}")
        var n = 2
        while (folder.exists()) folder = consoleDir.resolve("${PunicaLayout.MODS}/$id/${safeName(name)} ($n)").also { n++ }
        val canonical = folder.canonicalFile
        var written = 0
        ZipInputStream(open()).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (entry.isDirectory) continue
                val path = entry.name.replace('\\', '/').removePrefix(prefix)
                val target = folder.resolve(path).canonicalFile
                if (!target.path.startsWith(canonical.path + File.separator)) continue
                target.parentFile?.mkdirs()
                target.outputStream().use { zip.copyTo(it) }
                written++
            }
        }
        if (written > 0) writeName(folder, name)
        return written
    }

    /** Marks the mod or texture pack now in use at [kind]'s place with the name it was installed from. */
    fun nameInstalled(kind: Kind, name: String) {
        val id = gameId ?: return
        val place = if (kind == Kind.TEXTURES) consoleDir.resolve("${PunicaLayout.MODS}/${PunicaLayout.TEXTURES}/$id") else consoleDir.resolve("${PunicaLayout.MODS}/$id")
        if (place.isDirectory) writeName(place, name)
    }

    private fun setAside(mod: Mod) {
        val id = gameId ?: return
        val disabled = consoleDir.resolve("${PunicaLayout.MODS}/$DISABLED/${if (mod.kind == Kind.TEXTURES) "${PunicaLayout.TEXTURES}/" else ""}$id")
        var target = disabled.resolve(safeName(mod.name))
        var n = 2
        while (target.exists()) target = disabled.resolve("${safeName(mod.name)} (${n++})")
        writeName(mod.folder, mod.name)
        moveDir(mod.folder, target)
    }

    private fun moveDir(from: File, to: File) {
        to.parentFile?.mkdirs()
        if (!from.renameTo(to)) {
            if (!from.copyRecursively(to) || !from.deleteRecursively()) throw IOException("can't move $from to $to")
        }
    }

    private fun nameOf(folder: File, fallback: String? = null): String =
        folder.resolve(NAME).takeIf { it.isFile }?.readText()?.trim()?.ifEmpty { null } ?: fallback ?: folder.name

    private fun writeName(folder: File, name: String) {
        runCatching { folder.resolve(NAME).writeText(name) }
    }

    // the 3DS mod installer's record (ModInstaller.RECORD): the name a game's mod was installed from
    private fun recordName(id: String): String? = consoleDir.resolve("${PunicaLayout.MODS}/${ModInstaller.RECORD}").takeIf { it.isFile }
        ?.readLines()?.map { it.split('\t') }?.firstOrNull { it.size > 1 && it[0].equals(id, true) }?.get(1)?.ifEmpty { null }

    private companion object {
        const val REASON = ".reason"
        const val NAME = ".punica-name"
        const val DISABLED = "Disabled"
        val TEXTURE_PLATFORMS = setOf(RomPlatform.NDS, RomPlatform.N3DS, RomPlatform.GC_WII)
        val SWITCH_MOD_PARTS = setOf("romfs", "exefs", "cheats", "romfs_ext")
        val STAMP = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US)

        // a name usable as a folder's
        fun safeName(name: String) = name.replace(Regex("[\\\\/:*?\"<>|]"), "_").trim().ifEmpty { "_" }

        // a Wii title's low half: the game code's four letters in hexadecimal, as the Wii NAND names it
        fun hexOf(code: String) = code.toByteArray(Charsets.US_ASCII).joinToString("") { "%02x".format(it) }
    }
}
