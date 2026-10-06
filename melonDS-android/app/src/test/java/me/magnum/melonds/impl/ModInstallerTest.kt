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

    @Test
    fun modAndSaveAreExtractedIntoThe3dsFolder() {
        val folder = temp.newFolder("3DS")
        val result = install(zip("load/mods/$oras/romfs_ext/a/0/3/9.bps" to "patch", save to "save"), folder)
        assertEquals(ModInstaller.Result.Installed(setOf(oras), 2, 1), result)
        assertEquals("patch", File(folder, "load/mods/$oras/romfs_ext/a/0/3/9.bps").readText())
        assertEquals("save", File(folder, save).readText())
    }

    @Test
    fun theGamesPreviousModIsDeletedOtherGamesModsStay() {
        val folder = temp.newFolder("3DS")
        val stale = File(folder, "load/mods/$oras/romfs/a/0/1/3").apply { parentFile!!.mkdirs(); writeText("old") }
        val other = File(folder, "load/mods/0004000000055D00/romfs/x").apply { parentFile!!.mkdirs(); writeText("other") }
        install(zip("load/mods/$oras/romfs_ext/a/0/1/3.bps" to "new"), folder)
        assertFalse(stale.exists())
        assertTrue(File(folder, "load/mods/$oras/romfs_ext/a/0/1/3.bps").exists())
        assertEquals("other", other.readText())
    }

    @Test
    fun aZipOfTheModsFolderIsReadLikeItsContent() {
        val folder = temp.newFolder("3DS")
        val result = install(zip("Pomegrade_Sinnoh_r9/load/mods/$oras/romfs/a" to "a", "Pomegrade_Sinnoh_r9/$save" to "s"), folder)
        assertEquals(ModInstaller.Result.Installed(setOf(oras), 2, 1), result)
        assertTrue(File(folder, "load/mods/$oras/romfs/a").exists())
        assertTrue(File(folder, save).exists())
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
        val folder = temp.newFolder("3DS")
        val kept = File(folder, "load/mods/$oras/romfs/a").apply { parentFile!!.mkdirs(); writeText("kept") }
        assertEquals(ModInstaller.Result.NotAMod, install(zip("photo.png" to "x", "romfs/a/0/1/3" to "x"), folder))
        assertEquals("kept", kept.readText())
    }

    @Test
    fun notAZipIsNotAMod() {
        val folder = temp.newFolder("3DS")
        assertEquals(ModInstaller.Result.NotAMod, install("not a zip".toByteArray(), folder))
    }
}
