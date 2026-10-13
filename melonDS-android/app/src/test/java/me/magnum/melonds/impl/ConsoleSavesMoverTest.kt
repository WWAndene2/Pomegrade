package me.magnum.melonds.impl

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class ConsoleSavesMoverTest {

    @get:Rule
    val temp = TemporaryFolder()

    private fun file(dir: File, name: String, text: String = name) = File(dir, name).apply { parentFile?.mkdirs(); writeText(text) }

    @Test
    fun movesEachConsolesSavesIntoItsFolder() {
        val roms = temp.newFolder("Roms")
        val ds = File(temp.root, "DS")
        val gba = File(temp.root, "GBA")
        val pokemon = File(roms, "Pokemon Platinum")
        file(pokemon, "Pokemon Platinum.nds")
        file(pokemon, "Pokemon Platinum.sav")
        file(pokemon, "Pokemon Platinum.ml0")
        file(pokemon, "Pokemon Platinum.ml10")
        val emerald = File(roms, "Emerald")
        file(emerald, "Emerald.gba")
        file(emerald, "Emerald.sav")
        file(emerald, "Emerald.ss3")
        file(emerald, "Emerald.ss3.png")

        assertEquals(6, ConsoleSavesMover.moveAll(roms, ds, gba))

        assertTrue(File(ds, "Pokemon Platinum.sav").isFile)
        assertTrue(File(ds, "Pokemon Platinum.ml10").isFile)
        assertTrue(File(gba, "Emerald.ss3.png").isFile)
        // the ROMs stay in their game folders
        assertTrue(File(pokemon, "Pokemon Platinum.nds").isFile)
        assertTrue(File(emerald, "Emerald.gba").isFile)
        assertFalse(File(pokemon, "Pokemon Platinum.sav").exists())
    }

    @Test
    fun neverReplacesASaveTheConsoleFolderHas() {
        val roms = temp.newFolder("Roms")
        val ds = temp.newFolder("DS")
        val game = File(roms, "Game")
        file(game, "Game.nds")
        file(game, "Game.sav", "old")
        file(ds, "Game.sav", "kept")

        assertEquals(0, ConsoleSavesMover.moveAll(roms, ds, File(temp.root, "GBA")))

        assertEquals("kept", File(ds, "Game.sav").readText())
        assertEquals("old", File(game, "Game.sav").readText())
    }

    @Test
    fun leavesOtherConsolesAndOtherFilesAlone() {
        val roms = temp.newFolder("Roms")
        val game = File(roms, "Zelda")
        file(game, "Zelda.3ds")
        file(game, "Zelda.sav")
        val ds = File(roms, "Mario")
        file(ds, "Mario.nds")
        file(ds, "Mario.txt")

        assertEquals(0, ConsoleSavesMover.moveAll(roms, File(temp.root, "DS"), File(temp.root, "GBA")))
        assertTrue(File(game, "Zelda.sav").isFile)
        assertTrue(File(ds, "Mario.txt").isFile)
    }
}
