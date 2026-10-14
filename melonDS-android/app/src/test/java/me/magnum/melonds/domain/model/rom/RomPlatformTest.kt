package me.magnum.melonds.domain.model.rom

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
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
    fun gbaFilesAreRunByTheGbaCore() {
        listOf("game.gba", "GAME.GBA").forEach {
            assertEquals(it, RomPlatform.GBA, RomPlatform.fromFileName(it))
        }
    }

    @Test
    fun switchFilesAreRunByTheSwitchCore() {
        listOf("game.nsp", "game.xci", "game.nca", "homebrew.nro", "GAME.NSP").forEach {
            assertEquals(it, RomPlatform.SWITCH, RomPlatform.fromFileName(it))
        }
    }

    @Test
    fun switchFirmwareContentFilesAreNotGames() {
        listOf("042d62d198f4013897fa2c362c302aa0.cnmt.nca", "070a0a9b66d220a78bf7dc3c44dbba93.nca", "0A1B0C68454089B7E8CFAD67B78E4441.NCA").forEach {
            assertTrue(it, RomPlatform.isSwitchContentFile(it))
        }
        listOf("Pokemon Sword.nsp", "game.nca", "game.xci", "070a0a9b66d220a78bf7dc3c44dbba9.nca", "070a0a9b66d220a78bf7dc3c44dbba93.nsp").forEach {
            assertFalse(it, RomPlatform.isSwitchContentFile(it))
        }
    }

    @Test
    fun gameCubeAndWiiFilesAreRunByTheDolphinCore() {
        listOf("game.iso", "game.gcm", "game.rvz", "game.wia", "game.ciso", "game.gcz", "game.wbfs", "channel.wad", "GAME.RVZ").forEach {
            assertEquals(it, RomPlatform.GC_WII, RomPlatform.fromFileName(it))
        }
    }

    @Test
    fun onlyTheLastExtensionCounts() {
        assertEquals(RomPlatform.NDS, RomPlatform.fromFileName("my.3ds.nds"))
        assertEquals(RomPlatform.N3DS, RomPlatform.fromFileName("my.nds.3ds"))
        assertEquals(RomPlatform.GBA, RomPlatform.fromFileName("my.nds.gba"))
    }
}
