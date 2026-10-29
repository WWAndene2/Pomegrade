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
            "DS/System/DSi/bios7.bin", "DS/Mods/Textures/IRBF/t.png", "DS/Core/Traces/trace.txt",
            "GBA/Games/Emerald.gba", "GBA/Saves/Emerald.sav", "GBA/System/gba_bios.bin",
            "3DS/Games/Omega Ruby.3ds", "3DS/System/sdmc/Nintendo 3DS/game/save.bin", "3DS/System/aes_keys.txt",
            "3DS/States/0004000000055D00.01.cst", "3DS/Mods/0004000000055D00/romfs/a", "3DS/Core/config/config.ini",
            "Switch/Games/Pokemon Sword [0100ABF008968000].xci", "Switch/Saves/user/save/0000/1/sword.sav",
            "Switch/System/nand/system/Contents/system.nca", "Switch/System/prod.keys",
            "Switch/System/Firmware/${"%032x".format(1)}.cnmt.nca", "Switch/Mods/0100ABF008968000/mod/a",
            "Switch/Core/config/config.ini",
            "GC-Wii/Games/Zelda/disc1.rvz", "GC-Wii/System/Wii/title/00010000/game/data/banner.bin",
            "GC-Wii/System/GC/USA/IPL.bin", "GC-Wii/Saves/GameCube/MemoryCardA.USA.raw",
            "GC-Wii/Saves/GameCube/Card A/USA/01-GAME.gci", "GC-Wii/States/GAME.s01", "GC-Wii/Mods/Textures/GAME/t.png",
            "Settings/settings.json",
        ).forEach { assertTrue("missing $it", it in after) }

        // nothing else at the top level, every console with its six folders
        assertEquals(setOf(".punica-layout", "DS", "GBA", "3DS", "Switch", "GC-Wii", "Settings", "Backups"), root.list()!!.toSet())
        PunicaLayout.CONSOLES.forEach { console ->
            PunicaLayout.PARTS.forEach { assertTrue("$console/$it", File(File(root, console), it).isDirectory) }
            assertEquals(PunicaLayout.PARTS.toSet(), File(root, console).list()!!.toSet())
        }
        // Dolphin's GameCube folder is the layout's System/GC (its copy of the boot ROM the same file as the BIOS one)
        assertFalse(File(root, "GC-Wii/Core/GC").exists())
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

    @Test
    fun aGameWhoseNameIsTakenStaysInRomsUnmappedThenMovesOnceFree() {
        val root = earlierLayout()
        file(root, "Roms/Ruby.gba", "my ruby")
        file(root, "GBA/Games/Ruby.gba", "another ruby")
        file(root, "Roms/Kept/Kept.nds", "my kept")
        file(root, "DS/Games/Kept.nds", "another kept")
        val report = PunicaLayout.migrate(root)
        // still in Roms, still listed there; no entry mapped onto the other file
        assertEquals("my ruby", File(root, "Roms/Ruby.gba").readText())
        assertEquals("my kept", File(root, "Roms/Kept/Kept.nds").readText())
        assertEquals("another ruby", File(root, "GBA/Games/Ruby.gba").readText())
        assertFalse(File(root, "Roms/Ruby.gba") in report.games)
        assertFalse(File(root, "Roms/Kept/Kept.nds") in report.games)
        assertTrue(File(root, "Roms/Ruby.gba").path in report.left)

        // the owner frees the name: the next game list opening moves it
        File(root, "GBA/Games/Ruby.gba").delete()
        File(root, "DS/Games/Kept.nds").delete()
        val moved = mutableMapOf<File, File>()
        val again = PunicaLayout.moveLeftGames(root) { old, new -> moved[old] = new }
        assertEquals(File(root, "GBA/Games/Ruby.gba"), moved[File(root, "Roms/Ruby.gba")])
        assertEquals(File(root, "DS/Games/Kept.nds"), moved[File(root, "Roms/Kept/Kept.nds")])
        assertTrue(again.left.isEmpty())
        assertFalse(File(root, "Roms").exists())
    }

    @Test
    fun looseSavesBesideALooseGameGoToItsConsoleAndAreBackedUp() {
        val root = earlierLayout()
        file(root, "Roms/Ruby.gba", "ruby")
        file(root, "Roms/Ruby.sav", "ruby save")
        file(root, "Roms/Ruby.ss1", "ruby state")
        file(root, "Roms/Ruby.ss1.png", "ruby preview")
        file(root, "Roms/Diamond.nds", "diamond")
        file(root, "Roms/Diamond.ml0", "diamond state")
        file(root, "Roms/Orphan.sav", "no game")
        PunicaLayout.migrate(root)
        assertEquals("ruby save", File(root, "GBA/Saves/Ruby.sav").readText())
        assertEquals("ruby state", File(root, "GBA/States/Ruby.ss1").readText())
        assertEquals("ruby preview", File(root, "GBA/States/Ruby.ss1.png").readText())
        assertEquals("diamond state", File(root, "DS/States/Diamond.ml0").readText())
        assertTrue(File(root, "Backups").walkTopDown().any { it.name == "Orphan.sav" && it.path.contains("Unsorted-") })
        val backup = File(root, "Backups").listFiles { f -> f.name.startsWith("Saves-") }!!.single()
        assertEquals("ruby save", File(backup, "Roms/Ruby.sav").readText())
        assertEquals("emerald save", File(backup, "Roms/Emerald/Emerald.sav").readText())
    }

    @Test
    fun eachMovedGameIsToldAtOnceSoAStopPartwayLosesNone() {
        val root = earlierLayout()
        val told = mutableListOf<File>()
        // the game list fails while following the second game: the first is already followed
        runCatching {
            PunicaLayout.migrate(root) { old, _ ->
                told += old
                if (told.size == 2) throw IllegalStateException("stopped")
            }
        }
        assertEquals(2, told.size)
        assertFalse(PunicaLayout.isMigrated(root))
        assertFalse(told[0].exists())
        // run again: the rest moves, the games already moved are not told twice
        val again = PunicaLayout.migrate(root) { old, _ -> assertFalse(old in told); told += old }
        // a second run in the same second finds its saves' backup already made: only that is reported
        assertTrue(again.left.all { it.startsWith("backup: ") })
        assertTrue(PunicaLayout.isMigrated(root))
    }

    // the owner's folder on 29 October (screenshots): Azahar's default folders beside 3DS/Core (made before it was
    // pointed into the layout), its mods in Mods/mods; Eden's and Dolphin's defaults inside their Core
    private fun layoutWithStrays(): File {
        val root = temp.newFolder("Punica")
        PunicaLayout.migrate(root)
        file(root, "3DS/Core/cheats/000400000011C400.txt", "cheat in use")
        file(root, "3DS/cheats/000400000011C400.txt", "old cheat")
        file(root, "3DS/config/config.ini", "old config")
        File(root, "3DS/log").mkdirs()
        file(root, "3DS/sysdata/mac.txt", "mac")
        file(root, "3DS/shaders/opengl/transferable/000400000011C400.bin", "shaders")
        file(root, "3DS/Mods/mods/000400000011C400/romfs_ext/a/0/1/3.bps", "mod")
        file(root, "3DS/Mods/textures/000400000011C400/tex.png", "texture")
        File(root, "Switch/Core/nand").mkdirs()
        File(root, "Switch/Core/load").mkdirs()
        file(root, "Switch/Core/sdmc/Nintendo/Album/shot.jpg", "album")
        File(root, "GC-Wii/Core/StateSaves").mkdirs()
        file(root, "GC-Wii/Core/GC/SRAM.raw", "sram")
        file(root, "DS/Mods/CPUE/tex.png", "ds texture")
        file(root, "DS/Mods/dump/CPUE/d.png", "ds dump")
        return root
    }

    @Test
    fun tidyPutsBackWhatCoresLeftOutOfPlace() {
        val root = layoutWithStrays()
        val report = PunicaLayout.tidy(root)
        val all = files(root)
        for (f in listOf(
            "3DS/Mods/000400000011C400/romfs_ext/a/0/1/3.bps", "3DS/Mods/Textures/000400000011C400/tex.png",
            "3DS/System/mac.txt", "3DS/Core/shaders/opengl/transferable/000400000011C400.bin", "3DS/Core/config/config.ini",
            "Switch/System/sdmc/Nintendo/Album/shot.jpg", "GC-Wii/System/GC/SRAM.raw",
            "DS/Mods/Textures/CPUE/tex.png", "DS/Mods/Textures/dump/CPUE/d.png",
        )) assertTrue("$f in place", f in all)
        assertEquals("cheat in use", File(root, "3DS/Core/cheats/000400000011C400.txt").readText())
        // the older cheat, whose name the one in use has, is kept aside, not written over it
        assertTrue(all.any { it.startsWith("Backups/Conflicts-") && it.endsWith("3DS/cheats/000400000011C400.txt") })
        for (stray in listOf("3DS/cheats", "3DS/config", "3DS/log", "3DS/sysdata", "3DS/shaders", "3DS/Mods/mods", "3DS/Mods/textures",
            "Switch/Core/nand", "Switch/Core/load", "Switch/Core/sdmc", "GC-Wii/Core/StateSaves", "GC-Wii/Core/GC", "DS/Mods/CPUE", "DS/Mods/dump"))
            assertFalse("$stray gone", File(root, stray).exists())
        val consoles = File(root, "3DS").list()!!.filterNot { it.startsWith(".") }.toSet()
        assertEquals(PunicaLayout.PARTS.toSet(), consoles)
        assertTrue(report.left.isEmpty())
    }

    @Test
    fun tidyTwiceChangesNothingMore() {
        val root = layoutWithStrays()
        PunicaLayout.tidy(root)
        val once = files(root)
        val report = PunicaLayout.tidy(root)
        assertEquals(once, files(root))
        assertEquals(0, report.moved)
    }

    @Test
    fun tidyLosesNoContent() {
        val root = layoutWithStrays()
        val before = root.walkTopDown().filter { it.isFile && !it.name.startsWith(".") }.map { it.readText() }.sorted().toList()
        PunicaLayout.tidy(root)
        val after = root.walkTopDown().filter { it.isFile && !it.name.startsWith(".") && !it.path.contains("/Saves/") }.map { it.readText() }.sorted().toList()
        assertEquals(before, after)
    }

    @Test
    fun tidyLeavesAFolderNotInTheLayoutAlone() {
        val root = temp.newFolder("Punica")
        file(root, "3DS/cheats/a.txt", "a")
        PunicaLayout.tidy(root)
        assertTrue(File(root, "3DS/cheats/a.txt").isFile)
    }
}
