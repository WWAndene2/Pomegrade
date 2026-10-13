package me.magnum.melonds.impl

import me.magnum.melonds.domain.model.rom.RomPlatform
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class TexturePackInstallerTest {

    @Test
    fun `a pack in a folder named after the game gives the ID and loses the folder`() {
        val layout = TexturePackInstaller.layout(listOf("GALE01/tex1_64x64_abc.png", "GALE01/menu/tex2.dds"), RomPlatform.GC_WII)
        assertEquals("GALE01", layout.folderId)
        assertEquals(mapOf("GALE01/tex1_64x64_abc.png" to "tex1_64x64_abc.png", "GALE01/menu/tex2.dds" to "menu/tex2.dds"), layout.files)
    }

    @Test
    fun `a 3DS title ID folder is read in upper case`() {
        val layout = TexturePackInstaller.layout(listOf("0004000000030a00/a.png"), RomPlatform.N3DS)
        assertEquals("0004000000030A00", layout.folderId)
    }

    @Test
    fun `a top folder of another name is removed but gives no ID`() {
        val layout = TexturePackInstaller.layout(listOf("My HD Pack/a.png", "My HD Pack/b.png"), RomPlatform.NDS)
        assertNull(layout.folderId)
        assertEquals(setOf("a.png", "b.png"), layout.files.values.toSet())
    }

    @Test
    fun `files at the zip's root stay as they are`() {
        val layout = TexturePackInstaller.layout(listOf("a.png", "sub/b.png"), RomPlatform.NDS)
        assertNull(layout.folderId)
        assertEquals(mapOf("a.png" to "a.png", "sub/b.png" to "sub/b.png"), layout.files)
    }

    @Test
    fun `archivers' extra files are left out and don't count as a second top folder`() {
        val layout = TexturePackInstaller.layout(listOf("AMCE/a.png", "__MACOSX/AMCE/._a.png", "AMCE/.DS_Store"), RomPlatform.NDS)
        assertEquals("AMCE", layout.folderId)
        assertEquals(mapOf("AMCE/a.png" to "a.png"), layout.files)
    }
}
