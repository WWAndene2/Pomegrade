package me.magnum.melonds.impl

import org.junit.Assert.assertEquals
import org.junit.Test

class SwitchGameMetadataTest {

    @Test
    fun aFileNameLosesItsExtensionAndDumpTags() {
        assertEquals("Pokemon Sword", SwitchGameMetadata.cleanFileName("Pokemon Sword [0100ABF008968000].nsp"))
        assertEquals("Pokemon Sword", SwitchGameMetadata.cleanFileName("Pokemon Sword [0100ABF008968000][v0] (EU).xci"))
        assertEquals("Zelda BOTW", SwitchGameMetadata.cleanFileName("Zelda_BOTW.nsp"))
    }

    @Test
    fun aFileNameOfTagsOnlyKeepsThem() {
        assertEquals("[0100ABF008968000]", SwitchGameMetadata.cleanFileName("[0100ABF008968000].nsp"))
    }
}
