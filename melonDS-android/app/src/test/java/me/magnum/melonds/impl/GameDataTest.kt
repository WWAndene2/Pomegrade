package me.magnum.melonds.impl

import me.magnum.melonds.domain.model.rom.RomPlatform
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class GameDataTest {

    @get:Rule
    val temp = TemporaryFolder()

    private val oras = "000400000011C400"
    private val orasSave = "3DS/System/sdmc/Nintendo 3DS/${"0".repeat(32)}/${"0".repeat(32)}/title/00040000/0011c400/data/00000001/main"

    private fun file(root: File, path: String, text: String = path) = File(root, path).apply { parentFile?.mkdirs(); writeText(text) }

    private fun zip(vararg files: Pair<String, String>): ByteArray {
        val bytes = ByteArrayOutputStream()
        ZipOutputStream(bytes).use { zip -> files.forEach { (name, text) -> zip.putNextEntry(ZipEntry(name)); zip.write(text.toByteArray()); zip.closeEntry() } }
        return bytes.toByteArray()
    }

    @Test
    fun aDsSaveIsCopiedRestoredAndDeletedWithACopyFirst() {
        val root = temp.newFolder("Punica")
        val save = file(root, "DS/Saves/Platine.sav", "v1")
        val data = GameData(root, RomPlatform.NDS, "Platine.nds", "CPUF")
        assertEquals(listOf(save), data.saves())
        val first = data.backup("manual")!!
        assertEquals("manual", first.reason)
        save.writeText("v2")
        data.restore(first, "before restore")
        assertEquals("v1", save.readText())
        // the save as it was before the restore was copied
        assertTrue(data.backups().any { it.reason == "before restore" && it.folder.resolve("Saves/Platine.sav").readText() == "v2" })
        data.delete("before delete")
        assertFalse(save.exists())
        assertEquals(3, data.backups().size)
        assertNull(data.backup("nothing to copy"))
    }

    @Test
    fun a3dsSaveFolderGoesThroughAZipAndBack() {
        val root = temp.newFolder("Punica")
        file(root, orasSave, "omega ruby save")
        val data = GameData(root, RomPlatform.N3DS, "Omega Ruby.3ds", oras)
        val out = ByteArrayOutputStream()
        assertEquals(1, data.export(out))
        File(root, orasSave).writeText("changed")
        assertEquals(1, data.import({ out.toByteArray().inputStream() }, "before import"))
        assertEquals("omega ruby save", File(root, orasSave).readText())
        assertTrue(data.backups().any { it.reason == "before import" })
    }

    @Test
    fun anImportWritesOnlyThisGamesSave() {
        val root = temp.newFolder("Punica")
        file(root, "DS/Saves/Platine.sav", "mine")
        val data = GameData(root, RomPlatform.NDS, "Platine.nds", "CPUF")
        assertEquals(0, data.import({ zip("Saves/Other.sav" to "x", "../../escape" to "x").inputStream() }, "before"))
        assertEquals("mine", File(root, "DS/Saves/Platine.sav").readText())
        assertTrue(data.backups().isEmpty())
        assertEquals(1, data.import({ zip("Saves/Platine.sav" to "new", "Saves/Other.sav" to "x").inputStream() }, "before"))
        assertEquals("new", File(root, "DS/Saves/Platine.sav").readText())
        assertFalse(File(root, "DS/Saves/Other.sav").exists())
    }

    @Test
    fun switchSavesOfEveryProfileAreFound() {
        val root = temp.newFolder("Punica")
        file(root, "Switch/Saves/user/save/0000000000000000/AAAA/0100ABF008968000/save.bin", "a")
        file(root, "Switch/Saves/user/save/0000000000000000/BBBB/0100abf008968000/save.bin", "b")
        file(root, "Switch/Saves/user/save/0000000000000000/AAAA/0100000000010000/other.bin", "c")
        val data = GameData(root, RomPlatform.SWITCH, "Sword [0100ABF008968000].xci", "0100ABF008968000")
        assertEquals(2, data.saves().size)
    }

    @Test
    fun aGameCubeGamesGciFilesAreItsSave() {
        val root = temp.newFolder("Punica")
        file(root, "GC-Wii/Saves/GameCube/Card A/USA/01-GALE-SuperSmashBros.gci", "melee")
        file(root, "GC-Wii/Saves/GameCube/Card A/USA/01-GZLE-Zelda.gci", "zelda")
        val data = GameData(root, RomPlatform.GC_WII, "Melee.iso", "GALE01")
        assertEquals(listOf("01-GALE-SuperSmashBros.gci"), data.saves().map { it.name })
    }

    @Test
    fun aNew3dsModSetsTheOneInUseAsideAndEitherCanBeTurnedBackOn() {
        val root = temp.newFolder("Punica")
        file(root, orasSave, "save")
        file(root, "3DS/Mods/$oras/romfs/a", "kalos")
        file(root, "3DS/Mods/${ModInstaller.RECORD}", "$oras\tkalosfull\tsha\t1\t0\n")
        val data = GameData(root, RomPlatform.N3DS, "Omega Ruby.3ds", oras)
        assertEquals(listOf("kalosfull"), data.mods().filter { it.active }.map { it.name })
        assertTrue(data.setAsideBeforeInstall(GameData.Kind.MOD, "before mod"))
        // the new mod (as ModInstaller writes it)
        file(root, "3DS/Mods/$oras/romfs/a", "sinnoh")
        data.nameInstalled(GameData.Kind.MOD, "sinnoh")
        val mods = data.mods()
        assertEquals(listOf("sinnoh" to true, "kalosfull" to false), mods.map { it.name to it.active })
        data.setActive(mods.first { it.name == "kalosfull" }, true, "before mod")
        assertEquals("kalos", File(root, "3DS/Mods/$oras/romfs/a").readText())
        assertEquals(setOf("kalosfull" to true, "sinnoh" to false), data.mods().map { it.name to it.active }.toSet())
        assertTrue(data.backups().all { it.reason == "before mod" } && data.backups().size == 2)
        data.setActive(data.mods().first { it.active }, false, "before mod")
        assertFalse(File(root, "3DS/Mods/$oras").exists())
        assertEquals(2, data.mods().size)
    }

    @Test
    fun switchModsAreEachOnOrOff() {
        val root = temp.newFolder("Punica")
        val data = GameData(root, RomPlatform.SWITCH, "Sword [0100ABF008968000].xci", "0100ABF008968000")
        assertEquals(2, data.installSwitchMod({ zip("Better FPS/exefs/main.npdm" to "a", "Better FPS/romfs/x" to "b").inputStream() }, "Better FPS", "before"))
        assertEquals(1, data.installSwitchMod({ zip("romfs/y" to "c").inputStream() }, "HD", "before"))
        assertEquals(0, data.installSwitchMod({ zip("readme.txt" to "no").inputStream() }, "Nothing", "before"))
        assertTrue(File(root, "Switch/Mods/0100ABF008968000/Better FPS/exefs/main.npdm").isFile)
        assertEquals(setOf("Better FPS" to true, "HD" to true), data.mods().map { it.name to it.active }.toSet())
        data.setActive(data.mods().first { it.name == "HD" }, false, "before")
        assertEquals(setOf("Better FPS" to true, "HD" to false), data.mods().map { it.name to it.active }.toSet())
        data.setActive(data.mods().first { it.name == "HD" }, true, "before")
        assertTrue(File(root, "Switch/Mods/0100ABF008968000/HD/romfs/y").isFile)
        data.deleteMod(data.mods().first { it.name == "HD" }, "before")
        assertEquals(listOf("Better FPS"), data.mods().map { it.name })
    }

    @Test
    fun texturePacksOneInUseOthersAside() {
        val root = temp.newFolder("Punica")
        file(root, "DS/Mods/Textures/CPUF/a.png", "pack 1")
        val data = GameData(root, RomPlatform.NDS, "Platine.nds", "CPUF")
        assertTrue(data.setAsideBeforeInstall(GameData.Kind.TEXTURES, "before"))
        file(root, "DS/Mods/Textures/CPUF/a.png", "pack 2")
        data.nameInstalled(GameData.Kind.TEXTURES, "pack 2")
        data.setActive(data.mods().first { !it.active }, true, "before")
        assertEquals("pack 1", File(root, "DS/Mods/Textures/CPUF/a.png").readText())
        assertEquals(1, data.mods().count { it.active })
    }
}
