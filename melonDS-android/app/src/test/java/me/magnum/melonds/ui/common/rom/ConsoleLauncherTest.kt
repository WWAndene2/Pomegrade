package me.magnum.melonds.ui.common.rom

import me.magnum.melonds.domain.model.rom.RomPlatform
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Test

class ConsoleLauncherTest {

    @Test
    fun dsGamesStayInMelonDS() {
        assertNull(ConsoleLauncher.of(RomPlatform.NDS))
    }

    @Test
    fun eachImportedConsoleHasItsCore() {
        assertSame(N3dsLauncher, ConsoleLauncher.of(RomPlatform.N3DS))
        assertSame(GbaLauncher, ConsoleLauncher.of(RomPlatform.GBA))
        assertSame(SwitchLauncher, ConsoleLauncher.of(RomPlatform.SWITCH))
        assertSame(GcWiiLauncher, ConsoleLauncher.of(RomPlatform.GC_WII))
    }
}
