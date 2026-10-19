package me.magnum.melonds.impl

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class ModInstallerTest {

    @get:Rule val temp = TemporaryFolder()

    private val oras = "000400000011C400"
    private val save = "sdmc/Nintendo 3DS/0/0/title/00040000/0011c400/data/00000001/main"

    private fun zip(vararg files: Pair<String, String>): ByteArray {
        val bytes = ByteArrayOutputStream()
        ZipOutputStream(bytes).use { zip ->
            for ((name, text) in files) {
                zip.putNextEntry(ZipEntry(name))
                zip.write(text.toByteArray())
                zip.closeEntry()
            }
        }
        return bytes.toByteArray()
    }

    private fun install(zip: ByteArray, folder: File) = ModInstaller().install({ zip.inputStream() }, folder)

    // a 3DS folder in the Punica layout: Azahar tells it by its Core and System folders
    private fun layoutFolder() = temp.newFolder("3DS").apply { File(this, "Core").mkdirs(); File(this, "System").mkdirs() }

    @Test
    fun aFolderNotInTheLayoutGetsTheModWhereAzaharReadsItThere() {
        val folder = temp.newFolder("3DS")
        val result = install(zip("load/mods/$oras/romfs/a" to "a", save to "save"), folder)
        assertEquals(ModInstaller.Result.Installed(setOf(oras), 2, 1), result)
        assertEquals("a", File(folder, "load/mods/$oras/romfs/a").readText())
        assertEquals("save", File(folder, save).readText())
        assertFalse(File(folder, "Mods").exists())
    }

    @Test
    fun modAndSaveAreExtractedIntoThe3dsFolder() {
        val folder = layoutFolder()
        val result = install(zip("load/mods/$oras/romfs_ext/a/0/3/9.bps" to "patch", save to "save"), folder)
        assertEquals(ModInstaller.Result.Installed(setOf(oras), 2, 1), result)
        assertEquals("patch", File(folder, "Mods/mods/$oras/romfs_ext/a/0/3/9.bps").readText())
        assertEquals("save", File(folder, "System/$save").readText())
    }

    @Test
    fun theGamesPreviousModIsDeletedOtherGamesModsStay() {
        val folder = layoutFolder()
        val stale = File(folder, "Mods/mods/$oras/romfs/a/0/1/3").apply { parentFile!!.mkdirs(); writeText("old") }
        val other = File(folder, "Mods/mods/0004000000055D00/romfs/x").apply { parentFile!!.mkdirs(); writeText("other") }
        install(zip("load/mods/$oras/romfs_ext/a/0/1/3.bps" to "new"), folder)
        assertFalse(stale.exists())
        assertTrue(File(folder, "Mods/mods/$oras/romfs_ext/a/0/1/3.bps").exists())
        assertEquals("other", other.readText())
    }

    @Test
    fun aCopyLeftInTheOtherLayoutsModsFolderIsDeletedToo() {
        val folder = layoutFolder()
        // the folder Azahar reads before the layout: the core picks one when it starts
        val old = File(folder, "load/mods/$oras/romfs/a").apply { parentFile!!.mkdirs(); writeText("old") }
        File(folder, "Mods/mods/$oras/romfs/a").apply { parentFile!!.mkdirs(); writeText("older") }
        val result = install(zip("load/mods/$oras/romfs/b" to "new"), folder)
        assertFalse(old.exists())
        assertEquals(listOf("Mods/mods/$oras", "load/mods/$oras"), (result as ModInstaller.Result.Installed).removed)
        assertEquals("new", File(folder, "Mods/mods/$oras/romfs/b").readText())
    }

    @Test
    fun aZipIsRecognisedAsNewTheSameAgainOrAnUpdate() {
        val folder = layoutFolder()
        val first = zip("load/mods/$oras/romfs/a" to "1")
        val second = zip("load/mods/$oras/romfs/a" to "2")
        val installer = ModInstaller()
        val before = installer.recognise(installer.inspect { first.inputStream() }, folder).single()
        assertEquals(null, before.previous)
        assertTrue(before.copies.isEmpty())
        installer.install({ first.inputStream() }, folder, "vaniville", now = 1L)
        val again = installer.recognise(installer.inspect { first.inputStream() }, folder).single()
        assertTrue(again.sameZip)
        assertEquals("vaniville", again.previous!!.name)
        assertEquals(1L, again.previous.installedAt)
        assertEquals(listOf("Mods/mods/$oras"), again.copies)
        val update = installer.recognise(installer.inspect { second.inputStream() }, folder).single()
        assertFalse(update.sameZip)
        assertEquals("vaniville", update.previous!!.name)
        // the record sits beside Azahar's mods folder, not in it
        assertTrue(File(folder, "Mods/punica_installed_mods.tsv").isFile)
    }

    @Test
    fun aZipOfTheModsFolderIsReadLikeItsContent() {
        val folder = layoutFolder()
        val result = install(zip("Pomegrade_Sinnoh_r9/load/mods/$oras/romfs/a" to "a", "Pomegrade_Sinnoh_r9/$save" to "s"), folder)
        assertEquals(ModInstaller.Result.Installed(setOf(oras), 2, 1), result)
        assertTrue(File(folder, "Mods/mods/$oras/romfs/a").exists())
        assertTrue(File(folder, "System/$save").exists())
    }

    @Test
    fun entriesOutsideAModOrASaveAreNeverWritten() {
        val root = temp.newFolder("root")
        val folder = File(root, "3DS").apply { mkdirs() }
        val zip = zip(
            "load/mods/$oras/romfs/a" to "a",
            "load/mods/$oras/../../../escaped" to "x",
            "sdmc/../../escaped2" to "x",
            "readme.txt" to "x",
            "load/mods/notanid/romfs/b" to "x",
        )
        assertEquals(4, ModInstaller().inspect { zip.inputStream() }.skipped)
        assertEquals(ModInstaller.Result.Installed(setOf(oras), 1, 0), install(zip, folder))
        assertFalse(File(root, "escaped").exists())
        assertFalse(File(root, "escaped2").exists())
        assertFalse(File(folder, "readme.txt").exists())
    }

    @Test
    fun aZipWithNoModIsRefusedAndNothingIsTouched() {
        val folder = layoutFolder()
        val kept = File(folder, "Mods/mods/$oras/romfs/a").apply { parentFile!!.mkdirs(); writeText("kept") }
        assertEquals(ModInstaller.Result.NotAMod, install(zip("photo.png" to "x", "romfs/a/0/1/3" to "x"), folder))
        assertEquals("kept", kept.readText())
    }

    @Test
    fun notAZipIsNotAMod() {
        val folder = layoutFolder()
        assertEquals(ModInstaller.Result.NotAMod, install("not a zip".toByteArray(), folder))
    }
}
