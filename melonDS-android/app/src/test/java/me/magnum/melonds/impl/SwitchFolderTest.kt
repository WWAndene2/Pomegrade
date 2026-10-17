package me.magnum.melonds.impl

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class SwitchFolderTest {

    @get:Rule val temp = TemporaryFolder()

    @Test
    fun anotherAppsAndroidDataIsNotTheOwnersChoice() {
        val eden = File(temp.root, "Android/data/dev.eden.eden_emulator/files/nand").apply { mkdirs() }
        val own = File(temp.root, "Android/data/io.github.wwandene2.pomegrade/files").apply { mkdirs() }
        assertFalse(SwitchFolder.isOwnersChoice(eden, own))
        assertTrue(SwitchFolder.isOwnersChoice(File(own, "nand").apply { mkdirs() }, own))
    }

    @Test
    fun aMissingFolderIsNotTheOwnersChoice() {
        assertFalse(SwitchFolder.isOwnersChoice(File(temp.root, "gone/nand"), null))
        assertTrue(SwitchFolder.isOwnersChoice(temp.newFolder("SwitchNand"), null))
    }

    @Test
    fun insideMeansTheFolderOrBelowIt() {
        val folder = File(temp.root, "Switch")
        assertTrue(SwitchFolder.isInside(folder, folder))
        assertTrue(SwitchFolder.isInside(File(folder, "Core/nand"), folder))
        assertFalse(SwitchFolder.isInside(File(temp.root, "Switch2"), folder))
    }
}
