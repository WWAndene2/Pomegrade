package me.magnum.melonds.impl

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.io.File
import java.nio.file.Files

class GameFolderMoverTest {

    private lateinit var root: File
    private lateinit var downloads: File
    private lateinit var roms: File

    @Before
    fun setUp() {
        root = Files.createTempDirectory("pomegrade").toFile()
        downloads = File(root, "Download").apply { mkdirs() }
        roms = File(root, "Pomegrade/Roms").apply { mkdirs() }
    }

    @After
    fun tearDown() {
        root.deleteRecursively()
    }

    private fun file(dir: File, name: String, content: String = name) = File(dir, name).apply { writeText(content) }

    @Test
    fun movesTheRomWithItsSaveAndStatesIntoItsOwnFolder() {
        val rom = file(downloads, "Dragon Quest.nds")
        val sav = file(downloads, "Dragon Quest.sav")
        val state = file(downloads, "Dragon Quest.ml1")
        val missingState = File(downloads, "Dragon Quest.ml2")
        val other = file(downloads, "Other.sav")

        val folder = GameFolderMover.move(rom, listOf(sav, state, missingState), roms)

        assertEquals("Dragon Quest", folder)
        val gameDir = File(roms, "Dragon Quest")
        assertEquals("Dragon Quest.nds", File(gameDir, "Dragon Quest.nds").readText())
        assertEquals("Dragon Quest.sav", File(gameDir, "Dragon Quest.sav").readText())
        assertEquals("Dragon Quest.ml1", File(gameDir, "Dragon Quest.ml1").readText())
        assertFalse(rom.exists())
        assertFalse(sav.exists())
        assertFalse(File(gameDir, "Dragon Quest.ml2").exists())
        assertTrue(other.exists())
    }

    @Test
    fun aSecondGameWithTheSameFileNameGetsItsOwnFolder() {
        file(File(roms, "Game").apply { mkdirs() }, "Game.nds", "first")
        val rom = file(downloads, "Game.nds", "second")

        assertEquals("Game (2)", GameFolderMover.move(rom, emptyList(), roms))
        assertEquals("first", File(roms, "Game/Game.nds").readText())
        assertEquals("second", File(roms, "Game (2)/Game.nds").readText())
    }

    @Test
    fun aDsAndA3dsGameOfTheSameNameShareTheFolder() {
        file(File(roms, "Game").apply { mkdirs() }, "Game.nds")
        val rom = file(downloads, "Game.3ds")

        assertEquals("Game", GameFolderMover.move(rom, emptyList(), roms))
        assertTrue(File(roms, "Game/Game.nds").exists())
        assertTrue(File(roms, "Game/Game.3ds").exists())
    }

    @Test
    fun anExistingSaveInTheFolderIsNeverReplaced() {
        val gameDir = File(roms, "Game").apply { mkdirs() }
        file(gameDir, "Game.sav", "kept")
        // the ROM file name is free, its save's isn't
        File(gameDir, "Game.nds").delete()
        val rom = file(downloads, "Game.nds")
        val sav = file(downloads, "Game.sav", "other")

        assertEquals("Game", GameFolderMover.move(rom, listOf(sav), roms))
        assertEquals("kept", File(gameDir, "Game.sav").readText())
        assertEquals("other", sav.readText())
    }

    @Test
    fun aMissingRomMovesNothing() {
        val sav = file(downloads, "Gone.sav")

        assertNull(GameFolderMover.move(File(downloads, "Gone.nds"), listOf(sav), roms))
        assertTrue(sav.exists())
        assertFalse(File(roms, "Gone").exists())
    }

    @Test
    fun charactersAFolderNameCantHaveAreReplaced() {
        assertEquals("Zelda_ Spirit Tracks", GameFolderMover.folderNameFor(File("Zelda: Spirit Tracks.nds")))
        assertEquals("Game", GameFolderMover.folderNameFor(File(" .nds")))
    }

    @Test
    fun moveFileNeverOverwrites() {
        val from = file(downloads, "a.bin", "new")
        val to = file(roms, "a.bin", "old")

        assertFalse(GameFolderMover.moveFile(from, to))
        assertEquals("old", to.readText())
        assertTrue(from.exists())
    }
}
