package me.magnum.melonds.impl

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class PunicaLayoutTest {

    @get:Rule
    val temp = TemporaryFolder()

    private fun file(root: File, path: String, text: String = path) = File(root, path).apply { parentFile?.mkdirs(); writeText(text) }

    private fun files(root: File) = root.walkTopDown().filter { it.isFile }.map { it.relativeTo(root).invariantSeparatorsPath }.toSet()

    // the owner's folder as on 15 October (screenshots): old DS saves folder, Roms with game folders, a loose Switch
    // game and a firmware's NCAs one per folder, BIOS, and the 3DS, Switch and GameCube / Wii cores' own folders
    private fun earlierLayout(): File {
        val root = temp.newFolder("Punica")
        file(root, "DS/Dragon Quest Monsters - Joker.sav", "dqm save")
        file(root, "DS/Dragon Quest Monsters - Joker.ml1", "dqm state")
        file(root, "DS/Traces/trace.txt")
        file(root, "Roms/Pokemon - Version Platine/Pokemon - Version Platine.nds", "platine")
        file(root, "Roms/Pokemon Sword [0100ABF008968000].xci", "sword")
        file(root, "Roms/Omega Ruby/Omega Ruby.3ds", "omega ruby")
        file(root, "Roms/Emerald/Emerald.gba", "emerald")
        file(root, "Roms/Emerald/Emerald.sav", "emerald save")
        file(root, "Roms/Zelda/disc1.rvz", "disc 1")
        file(root, "Roms/Zelda/disc2.rvz", "disc 2")
        file(root, "Roms/readme.txt", "not a game")
        for (i in 0 until 3) {
            val hash = "%032x".format(i)
            file(root, "Roms/$hash/$hash.nca", "nca $i")
            file(root, "Roms/$hash.cnmt/$hash.cnmt.nca", "cnmt $i")
        }
        file(root, "BIOS/DS/bios7.bin", "ds bios7")
        file(root, "BIOS/DSi/bios7.bin", "dsi bios7")
        file(root, "BIOS/GBA/gba_bios.bin", "gba bios")
        file(root, "BIOS/3DS/aes_keys.txt", "aes keys")
        file(root, "BIOS/Switch/prod.keys", "prod keys")
        file(root, "BIOS/Switch/Firmware/${"%032x".format(0)}.nca", "nca 0")
        file(root, "BIOS/GC-Wii/USA/IPL.bin", "ipl")
        file(root, "3DS/sdmc/Nintendo 3DS/game/save.bin", "3ds save")
        file(root, "3DS/sysdata/aes_keys.txt", "aes keys")
        file(root, "3DS/states/0004000000055D00.01.cst", "3ds state")
        file(root, "3DS/load/mods/0004000000055D00/romfs/a", "3ds mod")
        file(root, "3DS/config/config.ini", "3ds config")
        file(root, "Switch/nand/user/save/0000/1/sword.sav", "sword save")
        file(root, "Switch/nand/system/Contents/system.nca", "switch system")
        file(root, "Switch/load/0100ABF008968000/mod/a", "switch mod")
        file(root, "Switch/config/config.ini", "switch config")
        file(root, "GC-Wii/Wii/title/00010000/game/data/banner.bin", "wii save")
        file(root, "GC-Wii/GC/MemoryCardA.USA.raw", "memory card")
        file(root, "GC-Wii/GC/USA/Card A/01-GAME.gci", "gci")
        file(root, "GC-Wii/GC/USA/IPL.bin", "ipl")
        file(root, "GC-Wii/StateSaves/GAME.s01", "gc state")
        file(root, "GC-Wii/Load/Textures/GAME/t.png", "gc texture")
        file(root, "Textures/IRBF/t.png", "ds texture")
        file(root, "Save state previews/12345/1.png", "preview")
        file(root, "Settings/settings.json", "settings")
        return root
    }

    @Test
    fun putsEveryFileInItsConsolesFolder() {
        val root = earlierLayout()
        val report = PunicaLayout.migrate(root)
        val after = files(root)

        listOf(
            "DS/Games/Pokemon - Version Platine.nds", "DS/Saves/Dragon Quest Monsters - Joker.sav",
            "DS/States/Dragon Quest Monsters - Joker.ml1", "DS/States/Previews/12345/1.png", "DS/System/bios7.bin",
            "DS/System/DSi/bios7.bin", "DS/Mods/IRBF/t.png", "DS/Core/Traces/trace.txt",
            "GBA/Games/Emerald.gba", "GBA/Saves/Emerald.sav", "GBA/System/gba_bios.bin",
            "3DS/Games/Omega Ruby.3ds", "3DS/System/sdmc/Nintendo 3DS/game/save.bin", "3DS/System/aes_keys.txt",
            "3DS/States/0004000000055D00.01.cst", "3DS/Mods/mods/0004000000055D00/romfs/a", "3DS/Core/config/config.ini",
            "Switch/Games/Pokemon Sword [0100ABF008968000].xci", "Switch/Saves/user/save/0000/1/sword.sav",
            "Switch/System/nand/system/Contents/system.nca", "Switch/System/prod.keys",
            "Switch/System/Firmware/${"%032x".format(1)}.cnmt.nca", "Switch/Mods/0100ABF008968000/mod/a",
            "Switch/Core/config/config.ini",
            "GC-Wii/Games/Zelda/disc1.rvz", "GC-Wii/System/Wii/title/00010000/game/data/banner.bin",
            "GC-Wii/System/USA/IPL.bin", "GC-Wii/Saves/GameCube/MemoryCardA.USA.raw",
            "GC-Wii/Saves/GameCube/Card A/USA/01-GAME.gci", "GC-Wii/States/GAME.s01", "GC-Wii/Mods/Textures/GAME/t.png",
            "GC-Wii/Core/GC/USA/IPL.bin", "Settings/settings.json",
        ).forEach { assertTrue("missing $it", it in after) }

        // nothing else at the top level, every console with its six folders
        assertEquals(setOf(".punica-layout", "DS", "GBA", "3DS", "Switch", "GC-Wii", "Settings", "Backups"), root.list()!!.toSet())
        PunicaLayout.CONSOLES.forEach { console ->
            PunicaLayout.PARTS.forEach { assertTrue("$console/$it", File(File(root, console), it).isDirectory) }
            assertEquals(PunicaLayout.PARTS.toSet(), File(root, console).list()!!.toSet())
        }
        assertTrue(report.left.isEmpty())
        assertEquals(File(root, "Switch/Games/Pokemon Sword [0100ABF008968000].xci"), report.games[File(root, "Roms/Pokemon Sword [0100ABF008968000].xci")])
    }

    @Test
    fun losesNoContent() {
        val root = earlierLayout()
        val before = root.walkTopDown().filter { it.isFile }.map { it.readText() }.toList()
        PunicaLayout.migrate(root)
        val after = root.walkTopDown().filter { it.isFile }.map { it.readText() }.toSet()
        // identical copies (the 3DS keys, a firmware NCA in both places) are kept once
        before.forEach { assertTrue("lost: $it", it in after) }
        assertTrue("not a game" in File(root, "Backups").walkTopDown().filter { it.isFile }.map { it.readText() }.toList())
    }

    @Test
    fun backsUpTheSavesFirst() {
        val root = earlierLayout()
        PunicaLayout.migrate(root)
        val backup = File(root, "Backups").listFiles { f -> f.name.startsWith("Saves-") }!!.single()
        assertEquals("dqm save", File(backup, "DS/Dragon Quest Monsters - Joker.sav").readText())
        assertEquals("sword save", File(backup, "Switch/save/0000/1/sword.sav").readText())
        assertEquals("memory card", File(backup, "GC-Wii/GC/MemoryCardA.USA.raw").readText())
    }

    @Test
    fun neverOverwritesAndRunsAgainHarmlessly() {
        val root = earlierLayout()
        file(root, "DS/Saves/Dragon Quest Monsters - Joker.sav", "a newer save already there")
        val first = PunicaLayout.migrate(root)
        assertEquals("a newer save already there", File(root, "DS/Saves/Dragon Quest Monsters - Joker.sav").readText())
        val aside = File(root, "Backups").walkTopDown().filter { it.isFile && it.path.contains("Conflicts-") }.map { it.readText() }.toList()
        assertTrue("dqm save" in aside)
        assertTrue(first.moved > 0)

        val again = PunicaLayout.migrate(root)
        assertEquals(0, again.moved)
        assertTrue(again.left.isEmpty())
        assertFalse(File(root, "Roms").exists())
    }

    @Test
    fun findsWhereAGameOfTheEarlierRomsWent() {
        val root = earlierLayout()
        PunicaLayout.migrate(root)
        // a game alone in its folder, a loose one, a game of several files that kept its folder
        assertEquals(File(root, "DS/Games/Pokemon - Version Platine.nds"),
            PunicaLayout.movedGame(root, File(root, "Roms/Pokemon - Version Platine/Pokemon - Version Platine.nds")))
        assertEquals(File(root, "Switch/Games/Pokemon Sword [0100ABF008968000].xci"),
            PunicaLayout.movedGame(root, File(root, "Roms/Pokemon Sword [0100ABF008968000].xci")))
        assertEquals(File(root, "GC-Wii/Games/Zelda/disc2.rvz"), PunicaLayout.movedGame(root, File(root, "Roms/Zelda/disc2.rvz")))
        // not a game of Roms, or a game that is not there
        assertEquals(null, PunicaLayout.movedGame(root, File(root, "Elsewhere/Game.nds")))
        assertEquals(null, PunicaLayout.movedGame(root, File(root, "Roms/Missing/Missing.nds")))
    }

    @Test
    fun firmwareLeavesRomsWhereverItWas() {
        val root = earlierLayout()
        val loose = "%032x".format(10)
        val hidden = "%032x".format(11)
        file(root, "Roms/$loose.nca", "loose nca")
        file(root, "Roms/$hidden/$hidden.nca", "nca beside a hidden file")
        file(root, "Roms/$hidden/.nomedia", "")
        for (i in 90 until 93) file(root, "Roms/Unpacked/${"%032x".format(i)}.nca", "game part $i")
        for (i in 20 until 80) file(root, "Roms/Firmware 19.0.1/${"%032x".format(i)}.nca", "unzipped $i")
        PunicaLayout.migrate(root)
        for (i in 20 until 80) assertEquals("unzipped $i", File(root, "Switch/System/Firmware/${"%032x".format(i)}.nca").readText())
        val firmware = File(root, "Switch/System/Firmware")
        assertEquals("loose nca", File(firmware, "$loose.nca").readText())
        assertEquals("nca beside a hidden file", File(firmware, "$hidden.nca").readText())
        // an unpacked game (a few NCAs together) stays a game
        assertEquals(3, File(root, "Switch/Games/Unpacked").listFiles()!!.size)
        // no firmware file taken for a game
        assertTrue(File(root, "Switch/Games").walkTopDown().none { it.isFile && it.name.endsWith(".nca") && !it.path.contains("Unpacked") })
        assertFalse(File(root, "Roms").exists())
    }
}
