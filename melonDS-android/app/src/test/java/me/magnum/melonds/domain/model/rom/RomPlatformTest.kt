package me.magnum.melonds.domain.model.rom

import org.junit.Assert.assertEquals
import org.junit.Test

class RomPlatformTest {

    @Test
    fun dsFilesAreRunByMelonDS() {
        listOf("game.nds", "game.dsi", "game.ids", "game.zip", "game.7z", "noextension").forEach {
            assertEquals(it, RomPlatform.NDS, RomPlatform.fromFileName(it))
        }
    }

    @Test
    fun n3dsFilesAreRunByTheN3dsCore() {
        listOf("game.3ds", "GAME.3DS", "game.cci", "game.cxi", "homebrew.3dsx", "game.z3dsx", "game.zcci", "game.zcxi").forEach {
            assertEquals(it, RomPlatform.N3DS, RomPlatform.fromFileName(it))
        }
    }

    @Test
    fun onlyTheLastExtensionCounts() {
        assertEquals(RomPlatform.NDS, RomPlatform.fromFileName("my.3ds.nds"))
        assertEquals(RomPlatform.N3DS, RomPlatform.fromFileName("my.nds.3ds"))
    }
}
