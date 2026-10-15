package me.magnum.melonds.impl

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
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
    fun moveFileNeverOverwrites() {
        val from = file(downloads, "a.bin", "new")
        val to = file(roms, "a.bin", "old")

        assertFalse(GameFolderMover.moveFile(from, to))
        assertEquals("old", to.readText())
        assertTrue(from.exists())
    }
}
