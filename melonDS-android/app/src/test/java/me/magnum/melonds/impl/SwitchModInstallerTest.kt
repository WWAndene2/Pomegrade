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

class SwitchModInstallerTest {

    @get:Rule val temp = TemporaryFolder()

    private val scarlet = "0100A3D008C5C000"

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

    private fun install(zip: ByteArray, folder: File, zipName: String = "Pack") =
        SwitchModInstaller().install({ zip.inputStream() }, folder, zipName)

    private fun mod(name: String) = SwitchModInstaller.Mod(scarlet, name)

    @Test
    fun edenLayoutIsExtractedUnderTheTitleAndModName() {
        val folder = temp.newFolder("Mods")
        val result = install(zip("$scarlet/Mali Scarlet/exefs/subsdk9" to "code"), folder)
        assertEquals(SwitchModInstaller.Result.Installed(setOf(mod("Mali Scarlet")), 1, emptySet()), result)
        assertEquals("code", File(folder, "$scarlet/Mali Scarlet/exefs/subsdk9").readText())
    }

    @Test
    fun atmosphereLayoutIsNamedAfterTheZip() {
        val folder = temp.newFolder("Mods")
        val result = install(zip("atmosphere/contents/$scarlet/romfs/a.bin" to "a"), folder, "Better Grass")
        assertEquals(SwitchModInstaller.Result.Installed(setOf(mod("Better Grass")), 1, emptySet()), result)
        assertEquals("a", File(folder, "$scarlet/Better Grass/romfs/a.bin").readText())
    }

    @Test
    fun aZipOfTheFolderIsReadLikeItsContent() {
        val folder = temp.newFolder("Mods")
        install(zip("release/$scarlet/Mali Scarlet/exefs/subsdk9" to "code"), folder)
        assertEquals("code", File(folder, "$scarlet/Mali Scarlet/exefs/subsdk9").readText())
    }

    @Test
    fun theSameModIsReplacedOtherModsOfTheGameStay() {
        val folder = temp.newFolder("Mods")
        val stale = File(folder, "$scarlet/Mali Scarlet/exefs/old").apply { parentFile!!.mkdirs(); writeText("old") }
        val other = File(folder, "$scarlet/60 FPS/exefs/patch.ips").apply { parentFile!!.mkdirs(); writeText("keep") }
        val result = install(zip("$scarlet/Mali Scarlet/exefs/subsdk9" to "new"), folder)
        assertEquals(SwitchModInstaller.Result.Installed(setOf(mod("Mali Scarlet")), 1, setOf(mod("Mali Scarlet"))), result)
        assertFalse(stale.exists())
        assertEquals("keep", other.readText())
    }

    @Test
    fun aZipWithoutAModIsRefusedAndNothingIsWritten() {
        val folder = temp.newFolder("Mods")
        assertEquals(SwitchModInstaller.Result.NotAMod, install(zip("readme.txt" to "hi", "Mali Scarlet/exefs/subsdk9" to "x"), folder))
        assertTrue(folder.list()!!.isEmpty())
    }

    @Test
    fun namesLeavingTheFolderAreSkipped() {
        val folder = temp.newFolder("Mods")
        val result = install(zip("$scarlet/Mod/exefs/a" to "a", "$scarlet/Mod/../../evil" to "x"), folder)
        assertEquals(SwitchModInstaller.Result.Installed(setOf(mod("Mod")), 1, emptySet()), result)
        assertFalse(File(folder.parentFile, "evil").exists())
    }

    @Test
    fun inspectListsTheModsWithoutWriting() {
        val folder = temp.newFolder("Mods")
        val contents = SwitchModInstaller().inspect({ zip("$scarlet/A/exefs/x" to "x", "notes.txt" to "n").inputStream() }, "Pack")
        assertEquals(setOf(mod("A")), contents.mods)
        assertEquals(1, contents.files)
        assertEquals(1, contents.skipped)
        assertTrue(folder.list()!!.isEmpty())
    }
}
